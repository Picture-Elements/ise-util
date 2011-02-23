/*
 * Copyright (c) 2004 Picture Elements, Inc.
 *    Stephen Williams <steve@picturel.com>
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

/*
 * The ISE board has on it a PCI-PCI bridge (function unit 0) and an
 * Address Translation Unit (function unit 1). It is the latter that
 * we are interested in, as it has the registers for communicating
 * with the i960 on the board. We ignore the bridge, as the BIOS takes
 * care of it for the most part.
 */

# include  "ucrif.h"
# include  "os.h"
# include  "ucrpriv.h"

/*
 * These are the registers that I use from the ISE board. The register
 * names are relative the board, so INBELLS are bells into the board,
 * etc.
 */
enum ATUOffsets {
      INBOUND0 = 0x10,
      INBOUND1 = 0x14,
      OUTBOUND0 = 0x18,
      OUTBOUND1 = 0x1c,

      INBELLS = 0x20,
      OUTBELLS = 0x2c,
      OISR = 0x30, /* Outbound Interrupt Status Register */
      OIMR = 0x34, /* Outbound Interrupt Mask Register */
};

/*
 * This structure contains the data needed to reload an ISE board that
 * has been powered off. It includes the volatile portions of the PCI
 * config space for both the bridge and the ATU.
 */
struct RestartData {
      unsigned br04;
      unsigned br0c;
      unsigned br18;
      unsigned br1c;
      unsigned br20;
      unsigned br24;
      unsigned br3c;

      unsigned dev04;
      unsigned dev0c;
      unsigned dev10;
      unsigned dev30;
      unsigned dev3c;
      unsigned dev40;
};



/*
 * This method is called to initialize the hardware in preparation for
 * all the other calls. This should clear any interrupts, then enable
 * only the OMR0/1 interrupts.
 */

void ise_init_hardware(struct Instance*xsp)
{
      __u32 tmp;
      dev_iowrite32(0x0fffffff, xsp->dev+OUTBELLS);
      dev_iowrite32(0, xsp->dev+INBOUND0);
      dev_iowrite32(0, xsp->dev+INBOUND1);
      dev_iowrite32(0, xsp->dev+OUTBOUND0);
      dev_iowrite32(0xfb, xsp->dev+OIMR);
      tmp = dev_ioread32(xsp->dev+OIMR);
      if ((tmp&0x7f) != 0x7b) {
	    printk("ise: error: OIMR wont stick. Wrote 0xfb, got %x\n", tmp);
      }
}

/*
 * This turns off interrupts, so the board can no longer abuse the
 * host. It need not totally reset the board.
 */
void ise_clear_hardware(struct Instance*xsp)
{
      dev_iowrite32(0xff, xsp->dev+OIMR);
      dev_iowrite32(0, xsp->dev+INBOUND0);
      dev_iowrite32(0, xsp->dev+INBOUND1);
      dev_iowrite32(0, xsp->dev+OUTBOUND0);
}


/*
 * The driver uses i960Rx message registers to send values. Send the
 * root table to the ISE board by writing to the INBOUND0 register the
 * value. The board will respond by copying the value to the OUTBOUND0
 * registers.
 */

void ise_set_root_table_base(struct Instance*xsp, __u32 value)
{     dev_iowrite32(value, xsp->dev+INBOUND0);
}

void ise_set_root_table_resp(struct Instance*xsp, __u32 value)
{     dev_iowrite32(0, xsp->dev+OUTBOUND0);
}

void ise_set_status_value(struct Instance*xsp, __u32 value)
{
      dev_iowrite32(value, xsp->dev+INBOUND1);
}

__u32 ise_get_root_table_ack(struct Instance*xsp)
{
      return dev_ioread32(xsp->dev+OUTBOUND0);
}

__u32 ise_get_status_resp(struct Instance*xsp)
{
      return dev_ioread32(xsp->dev+OUTBOUND1);
}

/*
 * This method sends the bell interrupts that are set in the
 * mask. This is done by writing into the out bells register, which is
 * different from the in bells. Note that on the i960RP, there are only
 * 31 doorbells, the highest bit is a NMI.
 */
void ise_set_bells(struct Instance*xsp, unsigned long mask)
{
      dev_iowrite32(mask & 0x7fffffff, xsp->dev+INBELLS);
}


/*
 * This method gets the bits of the currently set input bells, and
 * leaves those bits cleared in the bell register. That way, repeated
 * calls to dev_get_bells() will get a 1 only for bells that have been
 * set since the last call. The i960RP can only send 28 bells.
 *
 * In a shared interrupt environment, it is posible for a sibling
 * interrupt to cause the ISE interrupt handler to be called. If I see
 * that the doorbell interrupts are masked, then pretend that no
 * doorbells happen. The real interrupt will occur when the mask is
 * turned off.
 */
unsigned long ise_get_bells(struct Instance*xsp)
{
      if (dev_ioread32(xsp->dev+OIMR) & 4) return 0;
      else {
	    unsigned long mask = dev_ioread32(xsp->dev+OUTBELLS);
	    dev_iowrite32(mask, xsp->dev+OUTBELLS);
	    return mask & 0x0fffffff;
      }
}


/*
 * the dev_mask_irqs function masks the doorbell interrupts and
 * returns the current (before the mask) flag bits. The caller is
 * expected to pass that result back into the dev_unmask_irqs function
 * to restore interrupts.
 */
unsigned long ise_mask_irqs(struct Instance*xsp)
{
      unsigned long result = dev_ioread32(xsp->dev+OIMR);
      dev_iowrite32(result | 0x0f, xsp->dev+OIMR);
      return result;
}

void ise_unmask_irqs(struct Instance*xsp, unsigned long mask)
{
      dev_iowrite32(mask, xsp->dev+OIMR);
}


/*
 * It is easy to find the PCI-to-PCI brige part of the i960Rx given
 * the PCI handle for the MU. Just look at the numbers for the device,
 * and change the function number from 1 to 0.
 */
static struct pci_dev*i960rp_to_bridge(struct pci_dev*pci)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
      return pci->bus->self;
#else
      unsigned bus = pci->bus->number;
      unsigned dfn = pci->devfn;
	/* Clear the function bits. */
      dfn &= ~7;
	/* Lookup and return the PCI device by calculated devfn. */
      return pci_find_slot(bus, dfn);
#endif
}

void ise_soft_reset(struct Instance*xsp, int flag)
{
      int rc;
      unsigned char state;
      unsigned tmp[6];

      struct pci_dev*bridge_dev = i960rp_to_bridge(xsp->pci);

	/* Check to see if the board is already being held reset. If
	   it is, skip the reset step. Otherwise, save the volatile
	   configuration registers and set the processor to reset
	   state. */
      rc = pci_read_config_byte(bridge_dev, 0x40, &state);

      if (debug_flag&UCR_TRACE_UCRX)
	    printk("ucrx: read_config_byte returns rc=%d\n", rc);

      if ((state & 0x02) == 0) {
	      /* Save the volatile configuration registers. */
	    pci_read_config_dword(xsp->pci, 0x04, &tmp[0]);
	    pci_read_config_dword(xsp->pci, 0x0c, &tmp[1]);
	    pci_read_config_dword(xsp->pci, 0x10, &tmp[2]);
	    pci_read_config_dword(xsp->pci, 0x30, &tmp[3]);
	    pci_read_config_dword(xsp->pci, 0x3c, &tmp[4]);
	    pci_read_config_dword(xsp->pci, 0x40, &tmp[5]);


	      /* Send the reset command to the board. This goes to the
		 BRIDGE part of the device, which can control the i960
		 processor. */
	    pci_write_config_byte(bridge_dev, 0x40, flag? 0x20 : 0x00);


	      /* Restore the saved configuration registers. */
	    pci_write_config_dword(xsp->pci, 0x40, tmp[5]);
	    pci_write_config_dword(xsp->pci, 0x04, tmp[0]);
	    pci_write_config_dword(xsp->pci, 0x0c, tmp[1]);
	    pci_write_config_dword(xsp->pci, 0x10, tmp[2]);
	    pci_write_config_dword(xsp->pci, 0x30, tmp[3]);
	    pci_write_config_dword(xsp->pci, 0x3c, tmp[4]);
      }

}

/*
 * This function saves the current contents of the PCI configuration
 * registers, then turns the board off as much as is possible with
 * software.
 */
int isex_remove(struct Instance*xsp)
{
      struct pci_dev*bridge_dev;

      if (xsp->suspense)
	    return -EBUSY;

	/* Cannot remove the board if there are any open channels. */
      if (xsp->channels != 0)
	    return -EBUSY;

	/* On the ISE board, the I960Rx ALWAYS places the bridge at
	   the function unit one below the CPU. This gives me a nifty
	   way to calculate exactly the matching bridge unit */
      bridge_dev = i960rp_to_bridge(xsp->pci);

      xsp->suspense = kmalloc(sizeof(struct RestartData), GFP_KERNEL);

	/* Save the configuration space state of the DEVICE */
      pci_read_config_dword(xsp->pci, 0x04, &xsp->suspense->dev04);
      pci_read_config_dword(xsp->pci, 0x0c, &xsp->suspense->dev0c);
      pci_read_config_dword(xsp->pci, 0x10, &xsp->suspense->dev10);
      pci_read_config_dword(xsp->pci, 0x30, &xsp->suspense->dev30);
      pci_read_config_dword(xsp->pci, 0x3c, &xsp->suspense->dev3c);
      pci_read_config_dword(xsp->pci, 0x40, &xsp->suspense->dev40);

	/* Save the configuration space state of the BRIDGE */
      pci_read_config_dword(bridge_dev, 0x04, &xsp->suspense->br04);
      pci_read_config_dword(bridge_dev, 0x0c, &xsp->suspense->br0c);
      pci_read_config_dword(bridge_dev, 0x18, &xsp->suspense->br18);
      pci_read_config_dword(bridge_dev, 0x1c, &xsp->suspense->br1c);
      pci_read_config_dword(bridge_dev, 0x20, &xsp->suspense->br20);
      pci_read_config_dword(bridge_dev, 0x24, &xsp->suspense->br24);
      pci_read_config_dword(bridge_dev, 0x3c, &xsp->suspense->br3c);

	/* Activate the internal bus reset on the ISE board. This
	   resets the processor (and holds it reset) so that it is
	   certain to be quiescent from now on. */
      pci_write_config_byte(bridge_dev, 0x40, 0x20);

	/* Free all the frames that this board may have had. This can
	   be done directly because we know that the ISE board is held
	   reset and there is no need to synchronize tables. */
      { unsigned idx;
        for (idx = 0 ;  idx < 16 ;  idx += 1)
	      ucr_free_frame(xsp, idx);
      }

      return 0;
}

/*
 * This function undoes the work done by ucrx_remove. Gradually turn
 * the ISE board back on, and un-reset it. This is more complicated
 * then the remove, order matters.
 */
int isex_replace(struct Instance*xsp)
{
      struct pci_dev*bridge_dev;
      struct RestartData*suspense;

      if (! xsp->suspense)
	    return -EBUSY;

	/* On the ISE board, the I960Rx ALWAYS places the bridge at
	   the function unit one below the CPU. This gives me a nifty
	   way to calculate exactly the matching bridge unit */
      bridge_dev = i960rp_to_bridge(xsp->pci);

	/* First, make sure mastering and all error bits are off for
	   both configuration spaces. */
      pci_write_config_dword(bridge_dev, 0x04, 0xffff0000);
      pci_write_config_dword(xsp->pci,   0x04, 0xffff0000);

	/* Reload the mode and region registers. */
      pci_write_config_dword(bridge_dev, 0x0c, xsp->suspense->br0c);
      pci_write_config_dword(bridge_dev, 0x18, xsp->suspense->br18);
      pci_write_config_dword(bridge_dev, 0x1c, xsp->suspense->br1c);
      pci_write_config_dword(bridge_dev, 0x20, xsp->suspense->br20);
      pci_write_config_dword(bridge_dev, 0x24, xsp->suspense->br24);
      pci_write_config_dword(bridge_dev, 0x3c, xsp->suspense->br3c);

	/* Make extra sure the processor is off. */
      pci_write_config_byte(bridge_dev, 0x40, 0x20);

	/* Enable the bridge regions. */
      pci_write_config_dword(bridge_dev, 0x04, xsp->suspense->br04 & 0xffff);

	/* Now reload the device BAR registers. */
      pci_write_config_dword(xsp->pci, 0x40, xsp->suspense->dev40);
      pci_write_config_dword(xsp->pci, 0x0c, xsp->suspense->dev0c&0x00ffffff);
      pci_write_config_dword(xsp->pci, 0x10, xsp->suspense->dev10);
      pci_write_config_dword(xsp->pci, 0x30, xsp->suspense->dev30);
      pci_write_config_dword(xsp->pci, 0x3c, xsp->suspense->dev3c);

	/* Enable the bars. */
      pci_write_config_dword(xsp->pci, 0x04, xsp->suspense->dev04&0xffff);

	/* Turn of reference to any root table, just like reset. */
      dev_init_hardware(xsp);
      dev_set_root_table_base(xsp, 0);
      dev_set_root_table_resp(xsp, 0);

      suspense = xsp->suspense;
      xsp->suspense = 0;
      kfree(suspense);

	/* All done, release the processor. */
      pci_write_config_byte(bridge_dev, 0x40, 0x00);

      return 0;
}

void isex_diagnose_dump(struct Instance*xsp)
{
      isecons_log("ise%u: OIMR=%x, OISR=%x, ODR=%x\n", xsp->number,
		  dev_ioread32(xsp->dev+0x34), dev_ioread32(xsp->dev+0x30),
		  dev_ioread32(xsp->dev+0x2c));
      isecons_log("ise%u: OMR0=%x, OMR1=%x\n", xsp->number,
		  dev_ioread32(xsp->dev+0x18), dev_ioread32(xsp->dev+0x1c));

      isecons_log("ise%u: IIMR=%x, IISR=%x, IDR=%x\n", xsp->number,
		  dev_ioread32(xsp->dev+0x28), dev_ioread32(xsp->dev+0x24),
		  dev_ioread32(xsp->dev+0x20));
      isecons_log("ise%u: IMR0=%x, IMR1=%x\n", xsp->number,
		  dev_ioread32(xsp->dev+0x10), dev_ioread32(xsp->dev+0x14));
}

/*
 * $Log: dev_ise.c,v $
 * Revision 1.3  2008/09/08 22:29:50  steve
 *  Build for 2.6 kernel
 *
 * Revision 1.2  2004/07/14 17:41:57  steve
 *  Clear status register in restart/reset.
 *
 * Revision 1.1  2004/03/26 20:35:21  steve
 *  Add support for JSE device.
 *
 */
