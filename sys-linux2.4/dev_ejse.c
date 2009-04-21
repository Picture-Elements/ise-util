/*
 * Copyright (c) 2008 Picture Elements, Inc.
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
#ident "$Id: dev_ejse.c,v 1.7 2008/10/09 00:00:23 steve-icarus Exp $"

# include  "os.h"
# include  "ucrpriv.h"
# include  <linux/pci.h>
# include  <linux/pci_regs.h>

/*
 * The JSE uses an Intel 21555 bridge to provide the interface. In
 * particular, the scratchpad and doorbell registers are used to
 * communicate with the processor on the other side.
 *
 *    Mailbox0 (0xf0000)  - root table base
 *    Mailbox1 (0xf0004)  - root table response
 *    Mailbox2 (0xf0008)  - status value
 *    Mailbox3 (0xf000c)  - Status register response
 *
 * The bridge has only 16 bits of doorbells for each direction. They
 * are mapped from the ucr bellmasks like so:
 *
 *    ROOT_TABLE  0x00000001    --> 0x01
 *    STATUS      0x00000002    --> 0x02
 *    CHANGE      0x00000004    --> 0x04
 *    RESTART     0x40000000    --> 0x80
 *
 */

void ejse_init_hardware(struct Instance*xsp)
{
      struct pci_dev*bridge_dev;
      u8 tmp8;
      int rc;

      dev_iowrite32(0, xsp->dev+0xf0000);
      dev_iowrite32(0, xsp->dev+0xf0004);
      dev_iowrite32(0, xsp->dev+0xf0008);
	/* Set DoorBell.EBellIEn  */
      dev_iowrite32(0x0000ff00, xsp->dev+0x1000c);
	/* Globally enable interrupts */
      dev_iowrite32(1, xsp->dev+0x0000c);

	/* For diagnostic purposes, we like to fiddle with some of the
	   bridge configuration for the device. The bridge of an EJSE
	   is the bridge chip enbedded on the board itself. */
      bridge_dev = xsp->pci->bus->self;

      rc = pci_read_config_byte(bridge_dev, PCI_BRIDGE_CONTROL, &tmp8);
      tmp8 |= PCI_BRIDGE_CTL_PARITY;
      pci_write_config_byte(bridge_dev, PCI_BRIDGE_CONTROL, tmp8);
}

void ejse_clear_hardware(struct Instance*xsp)
{
      dev_iowrite32(0, xsp->dev+0xf0000);
      dev_iowrite32(0, xsp->dev+0xf0004);
      dev_iowrite32(0, xsp->dev+0xf0008);
	/* Globally disable interrupts */
      dev_iowrite32(1, xsp->dev+0x00008);
	/* Clear DoorBell.EBellIEn */
      dev_iowrite32(0x0000ff00, xsp->dev+0x10008);
}

/*
 * Accessor methods...
 */
void ejse_set_root_table_base(struct Instance*xsp, __u32 value)
{
      dev_iowrite32(value, xsp->dev+0xf0000);
}

void ejse_set_root_table_resp(struct Instance*xsp, __u32 value)
{
      dev_iowrite32(value, xsp->dev+0xf0004);
}

void ejse_set_status_value(struct Instance*xsp, __u32 value)
{
      dev_iowrite32(value, xsp->dev+0xf0008);
}

__u32 ejse_get_root_table_ack(struct Instance*xsp)
{
      return dev_ioread32(xsp->dev+0xf0004);
}

__u32 ejse_get_status_resp(struct Instance*xsp)
{
      return dev_ioread32(xsp->dev+0xf000c);
}

/*
 * This function activates the interrupt request bells to send a
 * request to the EJSE (to the secondary side).
 */
void ejse_set_bells(struct Instance*xsp, unsigned long mask)
{
      unsigned long use_mask = mask & 0x0000007f;
      if (mask & 0x40000000) use_mask |= 0x80;

	/* Secondary Set IRQ = use_mask */
      dev_iowrite32(use_mask << 16, xsp->dev+0x1000c);
}

/*
 * This function gets the interrupts being send to the PRIMARY side by
 * the JSE. Get only those bits that are not masked. While we're at
 * it, clear the interrupt so that the next get returns 0 for the same
 * bits. Thus, reading these bells must automatically clear the IRQ.
 */
unsigned long ejse_get_bells(struct Instance*xsp)
{
      unsigned long result;
      unsigned long tmp = dev_ioread32(xsp->dev+0x10000);
	/* mask == EBellIEn */
      unsigned mask = (tmp >> 8) & 0xff;
	/* bell = EBell */
      unsigned bell = (tmp >> 0) & 0xff;

	/* Ignore masked bits. They are not really interrupting. Note
	   that the "mask" from the hardware is actually enables. */
      unsigned masked_bell = bell & mask;

	/* Primary Clear IRQ == interrupting bells */
      dev_iowrite32(masked_bell, xsp->dev+0x10008);

      result = masked_bell & 0x007f;
      if (masked_bell & 0x80) result |= 0x40000000;

      return result;
}

unsigned long ejse_mask_irqs(struct Instance*xsp)
{
	/* Get the current enables status from Primary IRQ Mask */
      unsigned long result = dev_ioread32(xsp->dev+0x10000);
	/* Clear all bell enable bits. */
      dev_iowrite32(0x0000ff00, xsp->dev+0x10008);

	/* Return the masks. */
      return 0x0000ff00 & result;
}

void ejse_unmask_irqs(struct Instance*xsp, unsigned long mask)
{
	/* Enable bits in the mask */
      dev_iowrite32(mask&0x0000ff00, xsp->dev+0x1000c);
	/* Clear bits not in the mask. */
      dev_iowrite32((~mask)&0x0000ff00, xsp->dev+0x10008);
}

/*
 * Implement soft reset of the JSE processor.
 */
void ejse_soft_reset(struct Instance*xsp, int flag)
{
      printk(KERN_INFO "ise%u: ejse_soft_reset not implemented\n", xsp->number);
}

struct RestartData {
      u32 pci_state[16];
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)

# define do_pci_save_state(xsp) pci_save_state((xsp)->pci, (xsp)->suspense->pci_state)
# define do_pci_restore_state(xsp) pci_restore_state((xsp)->pci, (xsp)->suspense->pci_state)

#else

# define do_pci_save_state(xsp) do { \
	    pci_save_state((xsp)->pci);	 \
	    pci_save_state((xsp)->pci->bus->self); \
      } while (0)
# define do_pci_restore_state(xsp) do { \
	    pci_restore_state((xsp)->pci); \
	    pci_save_state((xsp)->pci->bus->self); \
      } while(0)

#endif

int ejsex_remove(struct Instance*xsp)
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

	/* Free all the frames that this board may have had. This can
	   be done directly because we know that the ISE board is held
	   reset and there is no need to synchronize tables. */
      { unsigned idx;
        for (idx = 0 ;  idx < 16 ;  idx += 1)
	      ucr_free_frame(xsp, idx);
      }

      return 0;
}

int ejsex_replace(struct Instance*xsp)
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


void ejsex_diagnose_dump(struct Instance*xsp)
{
      int rc;
      u16 status;

      isecons_log("ise%u: EGlbIntp=0x%08lx, EGlbWr=0x%08lx\n",
		  xsp->number,
		  (unsigned long)dev_ioread32(xsp->dev+0x00000),
		  (unsigned long)dev_ioread32(xsp->dev+0x00004));
      isecons_log("ise%u: DoorBell=0x%08lx\n",
		  xsp->number, (unsigned long)dev_ioread32(xsp->dev+0x10000));
      isecons_log("ise%u: SharedM0=0x%08x SharedM1=0x%08x\n",
		  xsp->number,
		  dev_ioread32(xsp->dev+0xf0000), dev_ioread32(xsp->dev+0xf0004));
      isecons_log("ise%u: SharedM2=0x%08x SharedM3=0x%08x\n",
		  xsp->number,
		  dev_ioread32(xsp->dev+0xf0008), dev_ioread32(xsp->dev+0xf000c));
      isecons_log("ise%u: SharedM4=0x%08x SharedM5=0x%08x\n",
		  xsp->number,
		  dev_ioread32(xsp->dev+0xf0010), dev_ioread32(xsp->dev+0xf0014));
      isecons_log("ise%u: SharedM6=0x%08x SharedM7=0x%08x\n",
		  xsp->number,
		  dev_ioread32(xsp->dev+0xf0018), dev_ioread32(xsp->dev+0xf001c));

	/* Check the status of the BJE FPGA. */
      rc = pci_read_config_word(xsp->pci, 6, &status);
      isecons_log("ise%u: PCI status=0x%04x\n", xsp->number, status);
      isecons_log("ise%u:    %s Data Parity Error\n", xsp->number,
		  (status&0x0100)? "  " : "NO");
      isecons_log("ise%u:    %s Received Target Abort\n", xsp->number,
		  (status&0x1000)? "  " : "NO");
      isecons_log("ise%u:    %s Received Master Abort\n", xsp->number,
		  (status&0x2000)? "  " : "NO");
      isecons_log("ise%u:    %s Signaled System Error\n", xsp->number,
		  (status&0x4000)? "  " : "NO");
      isecons_log("ise%u:    %s Detected Parity Error\n", xsp->number,
		  (status&0x8000)? "  " : "NO");
      rc = pci_write_config_word(xsp->pci, 6, status&0xf100);

      rc = pci_read_config_word(xsp->pci->bus->self, 0x1e, &status);
      isecons_log("ise%u: PCI BRIDGE status=0x%04x\n", xsp->number, status);
      isecons_log("ise%u:    %s Data Parity Error\n", xsp->number,
		  (status&0x0100)? "  " : "NO");
      isecons_log("ise%u:    %s Received Target Abort\n", xsp->number,
		  (status&0x1000)? "  " : "NO");
      isecons_log("ise%u:    %s Received Master Abort\n", xsp->number,
		  (status&0x2000)? "  " : "NO");
      isecons_log("ise%u:    %s Received System Error\n", xsp->number,
		  (status&0x4000)? "  " : "NO");
      isecons_log("ise%u:    %s Detected Parity Error\n", xsp->number,
		  (status&0x8000)? "  " : "NO");
      rc = pci_write_config_word(xsp->pci->bus->self, 0x1e, status&0xf100);

}

/*
 * $Log: dev_ejse.c,v $
 * Revision 1.7  2008/10/09 00:00:23  steve-icarus
 *  REMOVE saves state of bridge as well as BJE device.
 *
 * Revision 1.6  2008/10/08 18:34:55  steve-icarus
 *  Dump Tundra secondary status.
 *
 * Revision 1.5  2008/10/08 18:13:22  steve-icarus
 *  Dump PCI status register.
 *
 * Revision 1.4  2008/10/06 17:44:45  steve-icarus
 *  Implement powerdown for EJSE.
 *
 * Revision 1.3  2008/09/11 17:21:18  steve
 *  Get mask sense correct.
 *
 * Revision 1.2  2008/09/08 22:29:50  steve
 *  Build for 2.6 kernel
 *
 * Revision 1.1  2008/09/08 17:11:57  steve
 *  Support EJSE boards.
 *
 */

