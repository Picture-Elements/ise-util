/*
 * Copyright (c) 1997-2004 Picture Elements, Inc.
 *    Stephen Williams (steve@picturel.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */
#ident "$Id: ucrx.c,v 1.12 2008/12/01 16:17:02 steve-icarus Exp $"


/*
 * The ucrx device is a control access into the uCR target
 * board. Various board specific controls exist here. Each board has
 * an Instance, an ordinary device driver (which holds the channels)
 * and the control device, which is here.
 */


# include  "ucrif.h"
# include  "os.h"
# include  "ucrpriv.h"

static int ucrx_reset_board(struct Instance*xsp)
{
	/* Cannot reset suspended slot. */
      if (xsp->suspense)
	    return -EBUSY;

	/* Cannot reset the board if there are any open channels. */
      if (xsp->channels != 0)
	    return -EBUSY;

      if (debug_flag&UCR_TRACE_UCRX)
	    printk("ucrx: reset ISE\n");

	/* Put the board into soft reset state. */
      xsp->dev_ops->soft_reset(xsp, 1);

	/* Turn OFF any mention of any root table. This is normally
	   the case anyhow, but at powerup things may be a little
	   weird. Reinit the MU because the reset probably cleared
	   things. */
      dev_init_hardware(xsp);
      dev_set_root_table_base(xsp, 0);
      dev_set_root_table_resp(xsp, 0);

	/* Free all the frames that this board may have had. This can
	   be done directly because we know that the ISE board is held
	   reset and there is no need to synchronize tables. */
      { unsigned idx;
        for (idx = 0 ;  idx < 16 ;  idx += 1)
	      ucr_free_frame(xsp, idx);
      }

	/* Release the processor so it is free to execute the
	   bootprom. This works my removing the reset state bit. */
      xsp->dev_ops->soft_reset(xsp, 0);

      if (debug_flag&UCR_TRACE_UCRX)
	    printk("ucrx: reset done.\n");

      return 0;
}

static int ucrx_restart_board(struct Instance*xsp)
{
      if (xsp->suspense)
	    return -EBUSY;


	/* Cannot reset the board if there are any open channels. */
      if (xsp->channels != 0)
	    return -EBUSY;


      if (debug_flag&UCR_TRACE_UCRX)
	    printk("ucrx: restart ISE at bus=%u, dfn=%u\n",
		   xsp->pci->bus->number, xsp->pci->devfn);

	/* restart doorbell to the processor. Code on the processor
	   should notice this interrupt and restart itself. */
      dev_set_bells(xsp, 0x40000000);

	/* Clear outbound interrupts and the table pointer. (The ISE
	   board will loop waiting for the table pointer to clear. If
	   the table pointer already is cleared, then it will continue
	   with its reset.) */
      dev_init_hardware(xsp);

	/* Free all the frames that this board may have had. This is
	   safe to do because there are no longer any pointers to the
	   table, and the processor has been rebooted. */
      { unsigned idx;
        for (idx = 0 ;  idx < 16 ;  idx += 1)
	      ucr_free_frame(xsp, idx);
      }

      return 0;
}

/*
 * The ISE board bootprom uses the BIST bit to tell the host that the
 * application program is loaded and ready to go. If the BIST bit is
 * set, then start the BIST to cause the application to go.
 */
static int ucrx_run_program(struct Instance*xsp)
{
      unsigned state = 0;
      int rc;

      if (xsp->suspense)
	    return -EBUSY;

	/* Cannot reset the board if there are any open channels. */
      if (xsp->channels != 0)
	    return -EBUSY;

      state = dev_get_status_resp(xsp);
      if (state & 0x0001) {
	    if (0 == (state & 0x0002))
		  return -EAGAIN;

	    if (debug_flag&UCR_TRACE_UCRX)
		  printk("<1>isex%u: Run ISE with status message.\n",
			 xsp->number);

	    dev_set_status_value(xsp, 0x0002);
	    dev_set_bells(xsp, 0x0002);

	    rc = 0;

      } else {

	    pci_read_config_dword(xsp->pci, 0x0c, &state);

	    if (debug_flag&UCR_TRACE_UCRX)
		  printk("ucrx: Run ISE at bus=%u, dfn=%u state=%x\n",
			 xsp->pci->bus->number, xsp->pci->devfn, state);

	    if (! (state & 0x80000000))
		  return -EAGAIN;

	    printk("<1>isex%u: warning: using obsolete bist method "
		   "to run ise%u\n", xsp->number, xsp->number);

	    rc = pci_write_config_dword(xsp->pci, 0x0c, state|0x40000000);

      }

      if (debug_flag&UCR_TRACE_UCRX)
	    printk("isex%u: Run done, rc=%d\n", xsp->number, rc);

      return 0;
}

static int ucrx_diagnose(struct Instance*xsp, unsigned long arg)
{
      unsigned magic, lineno, idx;
      int cnt;
      char msg[128];

      if (xsp->suspense)
	    return -EBUSY;

      isecons_log("ucrx: diagnose, arg=%lu\n", arg);

      switch (arg) {

	  case 0:
	    magic = dev_ioread32(xsp->dev+0x50);
	    if (magic != 0xab0440ba) {
		  isecons_log("ucrx: No abort magic number.\n");
		  return 0;
	    }

	    lineno = dev_ioread32(xsp->dev+0x54);

	    cnt = dev_ioread32(xsp->dev+0x58);
	    if (cnt > (pci_resource_end(xsp->pci, 0)-pci_resource_start(xsp->pci, 0)-0x50)) {
		  isecons_log("ucrx: Invalid count: %u\n", cnt);
		  return 0;
	    }
	    if (cnt >= sizeof msg)
		  cnt = sizeof msg - 1;

	    memcpy_fromio(msg, (void*)(xsp->dev+0x5c), cnt);
	    msg[cnt] = 0;

	    isecons_log("ucrx: target panic msg: %s\n", msg);
	    isecons_log("ucrx: target panic code: %u (0x%x)\n", lineno, lineno);
	    break;

	  case 1:
	    for (idx = 0 ;  idx < DEVICE_COUNT_RESOURCE ;  idx += 1) {
		  if (xsp->pci->resource[idx].flags & IORESOURCE_MEM)
			isecons_log("ise%u: memory region: %lx - %lx "
				    "flags=%lx\n", xsp->number,
				    (unsigned long)xsp->pci->resource[idx].start,
				    (unsigned long)xsp->pci->resource[idx].end,
				    xsp->pci->resource[idx].flags);
	    }

	    isecons_log(DEVICE_NAME "%u: ROOT TABLE at %p(%x) "
		   "MAGIC=[%x:%x]\n", xsp->number, xsp->root,
		   xsp->root->self, xsp->root->magic,
		   xsp->root->self);

	    for (idx = 0 ;  idx < 16 ;  idx += 1) {
		  int use_page_count;
		  int pidx;

		  if (xsp->frame[idx] == 0)
			continue;

		  isecons_log(DEVICE_NAME "%u: FRAME %u magic=[%x:%x] "
			 "page_size=%uK page_count=%u\n", xsp->number,
			 idx, xsp->frame[idx]->magic,
			 xsp->frame[idx]->self,
			 xsp->frame[idx]->page_size/1024,
			 xsp->frame[idx]->page_count);

		  use_page_count = xsp->frame[idx]->page_count;
		  if (use_page_count > 128) use_page_count = 128;

		  for (pidx = 0 ; pidx < use_page_count ; pidx += 1) {
			isecons_log(DEVICE_NAME "%u:     page %3d: 0x%08x\n",
				    xsp->number, pidx,
				    xsp->frame[idx]->page[pidx]);
		  }
	    }

	    for (idx = 0 ;  idx < ROOT_TABLE_CHANNELS ;  idx += 1) {
		  if (xsp->root->chan[idx].magic == 0)
			continue;

		  isecons_log(DEVICE_NAME "%u: CHANNEL %u TABLE AT (%x) "
			 "MAGIC=%x\n", xsp->number, idx,
			 xsp->root->chan[idx].ptr,
			 xsp->root->chan[idx].magic);
	    }

	    if (xsp->channels) {
		  struct ChannelData*xpd = xsp->channels;
		  do {
			isecons_log(DEVICE_NAME "%u.%u: CHANNEL TABLE "
			       "MAGIC=[%x:%x]\n", xsp->number,
			       xpd->channel, xpd->table->magic,
			       xpd->table->self);

			isecons_log(DEVICE_NAME "%u.%u: OUT "
			       "(first=%u, next=%u, off=%u) "
			       "IN (first=%u, next=%u)\n", xsp->number,
			       xpd->channel, xpd->table->first_out_idx,
			       xpd->table->next_out_idx, xpd->out_off,
			       xpd->table->first_in_idx,
			       xpd->table->next_in_idx);

			for (idx = 0 ;  idx < CHANNEL_OBUFS ;  idx += 1)
			      isecons_log(DEVICE_NAME "%u.%u: obuf %u: "
				     "ptr=%x, count=%u\n",
				     xsp->number, xpd->channel, idx,
				     xpd->table->out[idx].ptr,
				     xpd->table->out[idx].count);

			for (idx = 0 ;  idx < CHANNEL_IBUFS ;  idx += 1)
			      isecons_log(DEVICE_NAME "%u.%u: ibuf %u: "
				     "ptr=%x, count=%u\n",
				     xsp->number, xpd->channel, idx,
				     xpd->table->in[idx].ptr,
				     xpd->table->in[idx].count);


			xpd = xpd->next;
		  } while (xsp->channels != xpd);
	    }

	    if (xsp->dev_ops->diagnose_dump)
		  xsp->dev_ops->diagnose_dump(xsp);

	    break;
      }

      return 0;
}

static int ucrx_set_debug_flag(unsigned flag)
{
      extern unsigned debug_flag;
      if (flag != debug_flag) {
	    debug_flag = flag;
	    printk("ucrx: set debug_flag to 0x%x\n", debug_flag);
      }
      return 0;
}

static int ucrx_get_debug_flag(void)
{
      extern unsigned debug_flag;
      return (int)debug_flag;
}

static int ucrx_timeout(struct Instance*xsp, unsigned long arg)
{
      struct ChannelData*xpd;
      struct ucrx_timeout_s args;

      copy_from_user(&args, (void*)arg, sizeof args);
      xpd = channel_by_id(xsp, args.id);
      if (xpd == 0)
	    return -EINVAL;

      if (args.read_timeout >= 0) {
	    xpd->read_timeout = (args.read_timeout * HZ + 999) / 1000;

      } else switch (args.read_timeout) {

	  case UCRX_TIMEOUT_OFF:
	    xpd->read_timeout = -1;
	    break;

	  case UCRX_TIMEOUT_FORCE:
	    xpd->read_timeout_flag = 1;
	    if (xpd->read_timing) {
		  del_timer(&xpd->read_timer);
		  xpd->read_timing = 0;
	    }
	    wake_up(&xsp->dispatch_sync);
	    break;

	  default:
	    return -EINVAL;
      }

      return 0;
}


int ucrx_open(struct Instance*xsp)
{
      return 0;
}

void ucrx_release(struct Instance*xsp)
{
}

int ucrx_ioctl(struct Instance*xsp, unsigned int cmd, unsigned long arg)
{
      switch (cmd) {

	  case UCRX_RESTART_BOARD:
	    return ucrx_restart_board(xsp);

	  case UCRX_RESET_BOARD:
	    return ucrx_reset_board(xsp);

	  case UCRX_RUN_PROGRAM:
	    return ucrx_run_program(xsp);

	  case UCRX_DIAGNOSE:
	    return ucrx_diagnose(xsp, arg);

	  case UCRX_SET_TRACE:
	    return ucrx_set_debug_flag(arg);

	  case UCRX_GET_TRACE:
	    return ucrx_get_debug_flag();

	  case UCRX_TIMEOUT:
	    return ucrx_timeout(xsp, arg);

	  case UCRX_REMOVE:
	    return xsp->dev_ops->soft_remove
		  ? xsp->dev_ops->soft_remove(xsp)
		  : -ENOTTY;

	  case UCRX_REPLACE:
	    return xsp->dev_ops->soft_replace
		  ? xsp->dev_ops->soft_replace(xsp)
		  : -ENOTTY;
      }

      return -ENOTTY;
}

/*
 * $Log: ucrx.c,v $
 * Revision 1.12  2008/12/01 16:17:02  steve-icarus
 *  More robust channel table handling.
 *
 * Revision 1.11  2008/10/08 18:14:04  steve-icarus
 *  Include some page addresses in frame dump.
 *
 * Revision 1.10  2008/09/11 17:57:32  steve
 *  Fix format type warnings.
 *
 * Revision 1.9  2008/09/08 22:29:50  steve
 *  Build for 2.6 kernel
 *
 * Revision 1.8  2004/03/26 20:35:22  steve
 *  Add support for JSE device.
 *
 * Revision 1.7  2002/05/08 20:09:41  steve
 *  Add the /proc/driver/isecons log file.
 *
 * Revision 1.6  2002/05/08 05:09:35  steve
 *  reset and restart need to recover PIALR register.
 *
 * Revision 1.5  2002/04/15 18:10:07  steve
 *  More register details in dump.
 *
 * Revision 1.4  2002/03/26 03:13:47  steve
 *  Implement hotswap ioctls.
 *
 * Revision 1.3  2001/08/14 22:25:30  steve
 *  Add SseBase device
 *
 * Revision 1.2  2001/08/02 17:11:48  steve
 *  Use status run method if available.
 *
 * Revision 1.1  2001/08/02 03:45:44  steve
 *  Linux 2.4 version of the driver.
 */

