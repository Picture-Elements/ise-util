#ifndef __ucrpriv_H
#define __ucrpriv_H
/*
 * Copyright (c) 2001-2004 Picture Elements, Inc.
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
#ident "$Id: ucrpriv.h,v 1.10 2009/02/06 19:21:30 steve Exp $"

# define DEVICE_NAME "ise"

/*
 * These are definitions that are likely needed throughout the driver
 * source for the ucr target. The os.h and target.h header files must
 * be included before this.
 */

# include  "ise_tables.h"

# define CHANNEL_IN_EMPTY(x) ((x)->table->first_in_idx == (x)->table->next_in_idx)
# define INCR_IN_IDX(x)  ((x) = ((x) + 1) % CHANNEL_IBUFS)

# define NEXT_IN_IDX(x) (((x) + 1) % CHANNEL_IBUFS)
# define NEXT_OUT_IDX(x) (((x) + 1) % CHANNEL_OBUFS)

/*
 * There are a few parameters that are local to an open file
 * descriptor, such as the current channel. They do not affect the
 * device at all, just the process's view of it. Instances of this are
 * created in the xxopen and released in the xxrelease.
 */
struct ChannelData {
      unsigned short channel;
      struct Instance*xsp;

	/* This is the address of the table for the channel, along
	   with its physical address. This structure is used to
	   communicate with the channel on the ISE board. */
      volatile struct channel_table*table;

	/* The buffers referenced by the table are addressed (in
	   kernel address space) by these pointers. */
      void* in[CHANNEL_IBUFS];
      void* out[CHANNEL_OBUFS];

      unsigned in_off;
      unsigned out_off;

	/* This is the reat timeout to use. */
      long read_timeout;
      struct timer_list read_timer;
      int read_timing, read_timeout_flag;

      struct ChannelData *next, *prev;
};

/*
 * These are the parameters common to all the open files on the
 * target. This reflects the state of the target as a whole.
 */
struct Instance {

      unsigned char number;

	/* These are the PCI bus address of the configuration
	   registers. There may be occasion to access configuration
	   space for the target device. */
      struct pci_dev*pci;

	/* This pointer, normaly null, points to restart data needed
	   to turn a board back on after it has been replaced in its
	   slot and powered on. */
      struct RestartData*suspense;

	/* This is the value passed to writeX by the dev_x
	   functions. The init gets whatever value is needed for write
	   to work (normally the value right out of the BAR
	   register). */
      unsigned long dev;
      const struct ise_ops_tab*dev_ops;

	/* kernel and bus addresses of the root table. */
      struct root_table* root;

      wait_queue_head_t root_sync;
      int root_timeout_flag;

	/* Kernel and bus addresses of the frame tables. There can be
	   up to 16 tables. frame_ref stores the number of mappings to
	   the frame, and prevents the frame being removed if it is
	   mapped. frame_order is the order used to allocate the
	   frame via __get_free_pages. It is needed for release. */
      struct frame_table*frame[16];
      unsigned frame_ref[16];
      int frame_order[16];

      wait_queue_head_t dispatch_sync;

	/* Channels are a bit more complicated, and have a driver
	   structure of their own. */
      struct ChannelData *channels;

      struct channel_table*channel_table_pool;
      __u32 channel_table_phys;
};

extern struct Instance*ucr_find_board_instance(unsigned id);

struct ise_ops_tab {
      const char*full_name;
# define ISE_FEATURE_FRAME64 0x0001
      int feature_flags;

      void (*init_hardware)(struct Instance*xsp);
      void (*clear_hardware)(struct Instance*xsp);
      unsigned long (*mask_irqs)(struct Instance*xsp);
      void (*unmask_irqs)(struct Instance*xsp, unsigned long mask);
      void (*set_root_table_base)(struct Instance*xsp, __u32 value);
      void (*set_root_table_resp)(struct Instance*xsp, __u32 value);
      __u32 (*get_root_table_ack)(struct Instance*xsp);
      __u32 (*get_status_resp)(struct Instance*xsp);
      void (*set_status_value)(struct Instance*xsp, __u32 value);

      void (*set_bells)(struct Instance*xsp, unsigned long mask);
      unsigned long (*get_bells)(struct Instance*xsp);

	/* soft_reset(xsp, 1) puts the board into reset state. */
      void (*soft_reset)(struct Instance*xsp, int flag);

	/* power on/off support functions. */
      int (*soft_remove)(struct Instance*xsp);
      int (*soft_replace)(struct Instance*xsp);

	/* Diagnose functions. */
      void (*diagnose_dump)(struct Instance*xsp);
};

  /* This is true if the device can access 64bit frame page addresses. */
# define dev_feature_frame64(xsp)  ((xsp)->dev_ops->feature_flags & ISE_FEATURE_FRAME64)

# define dev_clear_hardware(xsp)   (xsp)->dev_ops->clear_hardware(xsp)
# define dev_init_hardware(xsp)    (xsp)->dev_ops->init_hardware(xsp)
# define dev_mask_irqs(xsp)        (xsp)->dev_ops->mask_irqs(xsp)
# define dev_unmask_irqs(xsp,mask) (xsp)->dev_ops->unmask_irqs(xsp, mask)
# define dev_set_root_table_base(xsp,val) (xsp)->dev_ops->set_root_table_base(xsp,val)
# define dev_set_root_table_resp(xsp,val) (xsp)->dev_ops->set_root_table_resp(xsp,val)
# define dev_get_root_table_ack(xsp)   (xsp)->dev_ops->get_root_table_ack(xsp)
# define dev_get_status_resp(xsp)      (xsp)->dev_ops->get_status_resp(xsp)
# define dev_set_status_value(xsp,val) (xsp)->dev_ops->set_status_value(xsp,val)

# define dev_get_bells(xsp)      (xsp)->dev_ops->get_bells(xsp)
# define dev_set_bells(xsp,mask) (xsp)->dev_ops->set_bells(xsp,mask)

extern void set_time_delay(struct Instance*xsp,
			   struct timer_list*tim,
			   void (*fun)(unsigned long));
extern void cancel_time_delay(struct timer_list*tim);

extern unsigned debug_flag;

extern struct ChannelData* channel_by_id(struct Instance*xsp,
					 unsigned short id);


/*
 * These are the ucr functions for performing the various generic
 * operations of the uCR protocol.
 */
extern int  ucr_open(struct Instance*xsp, struct ChannelData*xpd);
extern void ucr_release(struct Instance*xsp, struct ChannelData*xpd);
extern long ucr_read(struct Instance*xsp, struct ChannelData*xpd,
		     char *bytes, unsigned long count, int block_flag);
extern long ucr_write(struct Instance*xsp, struct ChannelData*xpd,
		      const char*bytes, const unsigned long count);
extern int  ucr_ioctl(struct Instance*xsp, struct ChannelData*xpd,
		      unsigned cmd, unsigned long arg);
extern int ucr_irq(struct Instance*xsp);

extern int  ucrx_open(struct Instance*xsp);
extern void ucrx_release(struct Instance*xsp);
extern int  ucrx_ioctl(struct Instance*xsp, unsigned cmd, unsigned long arg);

/*
 * The generic code needs to make frames occasionally, but the task
 * itself is operating system specific. These methods manage the
 * workings of frames.
 */
extern int ucr_make_frame(struct Instance*xsp, unsigned id,
			  unsigned long size);
extern int ucr_free_frame(struct Instance*xsp, unsigned id);

/*
 * These methods manage the /proc/drivers/isecons entry.
 */
extern void isecons_log(const char*fmt, ...)
      __attribute__ ((format (printf, 1, 2)));

extern void isecons_init(void);
extern void isecons_release(void);

/*
 * These are generic functions that help with the management of the
 * instance structure.
 */
extern void ucr_init_instance(struct Instance*xsp);
extern void ucr_clear_instance(struct Instance*xsp);

/*
 * Device hardware manipulation functions.
 */
extern void ise_init_hardware(struct Instance*xsp);
extern void ise_clear_hardware(struct Instance*xsp);
extern unsigned long ise_mask_irqs(struct Instance*xsp);
extern void ise_unmask_irqs(struct Instance*xsp, unsigned long mask);
extern void ise_set_root_table_base(struct Instance*xsp, __u32 value);
extern void ise_set_root_table_resp(struct Instance*xsp, __u32 value);
extern __u32 ise_get_root_table_ack(struct Instance*xsp);
extern __u32 ise_get_status_resp(struct Instance*xsp);
extern void ise_set_status_value(struct Instance*xsp, __u32 value);

extern void ise_set_bells(struct Instance*xsp, unsigned long mask);
extern unsigned long ise_get_bells(struct Instance*xsp);

extern void ise_soft_reset(struct Instance*xsp, int flag);
extern int isex_remove(struct Instance*xsp);
extern int isex_replace(struct Instance*xsp);

extern void isex_diagnose_dump(struct Instance*xsp);


extern void jse_init_hardware(struct Instance*xsp);
extern void jse_clear_hardware(struct Instance*xsp);
extern unsigned long jse_mask_irqs(struct Instance*xsp);
extern void jse_unmask_irqs(struct Instance*xsp, unsigned long mask);
extern void jse_set_root_table_base(struct Instance*xsp, __u32 value);
extern void jse_set_root_table_resp(struct Instance*xsp, __u32 value);
extern __u32 jse_get_root_table_ack(struct Instance*xsp);
extern __u32 jse_get_status_resp(struct Instance*xsp);
extern void jse_set_status_value(struct Instance*xsp, __u32 value);

extern void jse_set_bells(struct Instance*xsp, unsigned long mask);
extern unsigned long jse_get_bells(struct Instance*xsp);

extern void jse_soft_reset(struct Instance*xsp, int flag);
extern int jsex_remove(struct Instance*xsp);
extern int jsex_replace(struct Instance*xsp);

extern void jsex_diagnose_dump(struct Instance*xsp);

extern void ejse_init_hardware(struct Instance*xsp);
extern void ejse_clear_hardware(struct Instance*xsp);
extern unsigned long ejse_mask_irqs(struct Instance*xsp);
extern void ejse_unmask_irqs(struct Instance*xsp, unsigned long mask);
extern void ejse_set_root_table_base(struct Instance*xsp, __u32 value);
extern void ejse_set_root_table_resp(struct Instance*xsp, __u32 value);
extern __u32 ejse_get_root_table_ack(struct Instance*xsp);
extern __u32 ejse_get_status_resp(struct Instance*xsp);
extern void ejse_set_status_value(struct Instance*xsp, __u32 value);

extern void ejse_set_bells(struct Instance*xsp, unsigned long mask);
extern unsigned long ejse_get_bells(struct Instance*xsp);

extern void ejse_soft_reset(struct Instance*xsp, int flag);
extern int ejsex_remove(struct Instance*xsp);
extern int ejsex_replace(struct Instance*xsp);

extern void ejsex_diagnose_dump(struct Instance*xsp);

/*
 * $Log: ucrpriv.h,v $
 * Revision 1.10  2009/02/06 19:21:30  steve
 *  Add read/write console for the ISE driver.
 *
 * Revision 1.9  2009/02/05 17:06:57  steve
 *  Support for frame64 frame table pointers.
 *
 * Revision 1.8  2008/12/01 16:17:02  steve-icarus
 *  More robust channel table handling.
 *
 * Revision 1.7  2008/09/08 17:11:57  steve
 *  Support EJSE boards.
 *
 * Revision 1.6  2008/08/25 22:27:31  steve
 *  Better portability in the Linux universe.
 *
 * Revision 1.5  2004/03/26 20:35:22  steve
 *  Add support for JSE device.
 *
 * Revision 1.4  2002/05/08 20:09:41  steve
 *  Add the /proc/driver/isecons log file.
 *
 * Revision 1.3  2002/05/08 05:09:35  steve
 *  reset and restart need to recover PIALR register.
 *
 * Revision 1.2  2002/03/26 03:13:47  steve
 *  Implement hotswap ioctls.
 *
 * Revision 1.1  2001/08/02 03:45:44  steve
 *  Linux 2.4 version of the driver.
 *
 */


#endif
