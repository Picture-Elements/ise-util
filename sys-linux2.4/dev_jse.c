/*
 * Copyright (c) 2004 Picture Elements, Inc.
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
#ident "$Id: dev_jse.c,v 1.3 2008/09/08 22:29:50 steve Exp $"

# include  "os.h"
# include  "ucrpriv.h"

/*
 * The JSE uses an Intel 21555 bridge to provide the interface. In
 * particular, the scratchpad and doorbell registers are used to
 * communicate with the processor on the other side.
 *
 *    ScratchPad0 (0xa8)  - root table base
 *    ScratchBad1 (0xac)  - root table response
 *    ScratchPad2 (0xb0)  - status value
 *    ScratchBad3 (0xb4)  - Status register response
 *
 * The bridge has only 16 bits of doorbells for each direction. They
 * are mapped from the ucr bellmasks like so:
 *
 *    ROOT_TABLE  0x00000001    --> 0x0001
 *    STATUS      0x00000002    --> 0x0002
 *    CHANGE      0x00000004    --> 0x0004
 *    RESTART     0x40000000    --> 0x4000
 *
 */

void jse_init_hardware(struct Instance*xsp)
{
      dev_iowrite32(0, xsp->dev+0xa8);
      dev_iowrite32(0, xsp->dev+0xac);
      dev_iowrite32(0, xsp->dev+0xb0);
	/* Unmask interrupts. */
      dev_iowrite16(0xffff, xsp->dev+0xa0);
}

void jse_clear_hardware(struct Instance*xsp)
{
      dev_iowrite32(0, xsp->dev+0xa8);
      dev_iowrite32(0, xsp->dev+0xac);
      dev_iowrite32(0, xsp->dev+0xb0);
	/* Mask interrupts. */
      dev_iowrite16(0xffff, xsp->dev+0xa4);
}

/*
 * Accessor methods...
 */
void jse_set_root_table_base(struct Instance*xsp, __u32 value)
{
      dev_iowrite32(value, xsp->dev+0xa8);
}

void jse_set_root_table_resp(struct Instance*xsp, __u32 value)
{
      dev_iowrite32(value, xsp->dev+0xac);
}

void jse_set_status_value(struct Instance*xsp, __u32 value)
{
      dev_iowrite32(value, xsp->dev+0xb0);
}

__u32 jse_get_root_table_ack(struct Instance*xsp)
{
      return dev_ioread32(xsp->dev+0xac);
}

__u32 jse_get_status_resp(struct Instance*xsp)
{
      return dev_ioread32(xsp->dev+0xb4);
}

/*
 * This function activates the interrupt request bells to send a
 * request to the JSE (to the secondary side).
 */
void jse_set_bells(struct Instance*xsp, unsigned long mask)
{
      unsigned short use_mask = mask & 0x000000ff;
      if (mask & 0x40000000) use_mask |= 0x4000;

	/* Secondary Set IRQ = use_mask */
      dev_iowrite16(use_mask, xsp->dev+0x09e);
}

/*
 * This function gets the interrupts being send to the PRIMARY side by
 * the JSE. Get only those bits that are not masked. While we're at
 * it, clear the interrupt so that the next get returns 0 for the same
 * bits. Thus, reading these bells must automatically clear the IRQ.
 */
unsigned long jse_get_bells(struct Instance*xsp)
{
      unsigned long result;
	/* mask == Primary IRQ mask */
      unsigned mask = dev_ioread16(xsp->dev+0x0a0);
	/* bell = Primary IRQ */
      unsigned bell = dev_ioread16(xsp->dev+0x098);

	/* Ignore masked bits. They are not really interrupting. */
      bell = bell & ~mask;

	/* Primary Clear IRQ == interrupting bells */
      dev_iowrite16(bell, xsp->dev+0x098);

      result = bell & 0x00ff;
      if (bell & 0x4000) result |= 0x40000000;

      return result;
}

unsigned long jse_mask_irqs(struct Instance*xsp)
{
	/* Get the current mask status from Primary IRQ Mask */
      unsigned long result = dev_ioread16(xsp->dev+0x0a4);
	/* Set all bell mask bits. */
      dev_iowrite16(0xffff, xsp->dev+0x0a4);

      return result;
}

void jse_unmask_irqs(struct Instance*xsp, unsigned long mask)
{
	/* Set bits that need to be set */
      dev_iowrite16(mask&0xffff, xsp->dev+0x0a4);
	/* Clear bits that need to be cleared. */
      dev_iowrite16((~mask)&0xffff, xsp->dev+0x0a0);
}

/*
 * Implement soft reset of the JSE processor.
 */
void jse_soft_reset(struct Instance*xsp, int flag)
{
      int rc;
      unsigned char state;

      rc = pci_read_config_byte(xsp->pci, 0xd8, &state);

      if (flag)
	    state |= 0x01;
      else
	    state &= ~0x01;

      rc = pci_write_config_byte(xsp->pci, 0xd8, state);
}

struct RestartData {
      u32 pci_state[16];
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
# define do_pci_save_state(xsp) pci_save_state((xsp)->pci, (xsp)->suspense->pci_state)
# define do_pci_restore_state(xsp) pci_restore_state((xsp)->pci, (xsp)->suspense->pci_state)
#else
# define do_pci_save_state(xsp) pci_save_state((xsp)->pci)
# define do_pci_restore_state(xsp) pci_restore_state((xsp)->pci)
#endif


int jsex_remove(struct Instance*xsp)
{

      if (xsp->suspense)
	    return -EBUSY;

	/* Cannot remove the board if there are any open channels. */
      if (xsp->channels != 0)
	    return -EBUSY;

      xsp->suspense = kmalloc(sizeof(struct RestartData), GFP_KERNEL);
      if (xsp->suspense == 0)
	    return -ENOMEM;

      do_pci_save_state(xsp);
      jse_soft_reset(xsp, 1);

	/* Free all the frames that this board may have had. This can
	   be done directly because we know that the ISE board is held
	   reset and there is no need to synchronize tables. */
      { unsigned idx;
        for (idx = 0 ;  idx < 16 ;  idx += 1)
	      ucr_free_frame(xsp, idx);
      }

      return 0;
}

int jsex_replace(struct Instance*xsp)
{
      struct RestartData*suspense;

      if (! xsp->suspense)
	    return -EBUSY;

      jse_soft_reset(xsp, 1);
      do_pci_restore_state(xsp);
      jse_soft_reset(xsp, 0);

	/* Turn of reference to any root table, just like reset. */
      dev_init_hardware(xsp);
      dev_set_root_table_base(xsp, 0);
      dev_set_root_table_resp(xsp, 0);

      suspense = xsp->suspense;
      xsp->suspense = 0;
      kfree(suspense);

	/* All done, release the processor. */
      jse_soft_reset(xsp, 0);

      return 0;
}


void jsex_diagnose_dump(struct Instance*xsp)
{
      isecons_log("ise%u: SCRATCH0=0x%08x SCRATCH1=0x%08x\n",
		  xsp->number,
		  dev_ioread32(xsp->dev+0x0a8), dev_ioread32(xsp->dev+0x0ac));
      isecons_log("ise%u: SCRATCH2=0x%08x SCRATCH3=0x%08x\n",
		  xsp->number,
		  dev_ioread32(xsp->dev+0x0b0), dev_ioread32(xsp->dev+0x0b4));
      isecons_log("ise%u: IRQ=0x%08x MASK=0x%08x\n",
		  xsp->number,
		  dev_ioread32(xsp->dev+0x098), dev_ioread32(xsp->dev+0x0a0));
}

/*
 * $Log: dev_jse.c,v $
 * Revision 1.3  2008/09/08 22:29:50  steve
 *  Build for 2.6 kernel
 *
 * Revision 1.2  2004/07/14 17:57:30  steve
 *  Clear status register when restarting board.
 *
 * Revision 1.1  2004/03/26 20:35:21  steve
 *  Add support for JSE device.
 *
 */

