#ifndef __os_linux_H
#define __os_linux_H
/*
 * Copyright (c) 1997 Picture Elements, Inc.
 *    Stephen Williams (steve@picturel.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version. In order to redistribute the software in
 *    binary form, you will need a Picture Elements Binary Software
 *    License.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 *  ---
 *    You should also have recieved a copy of the Picture Elements
 *    Binary Software License offer along with the source. This offer
 *    allows you to obtain the right to redistribute the software in
 *    binary (compiled) form. If you have not received it, contact
 *    Picture Elements, Inc., 777 Panoramic Way, Berkeley, CA 94704.
 */
#ident "$Id: os.h,v 1.7 2009/02/05 17:06:57 steve Exp $"

# include  <linux/module.h>
# include  <linux/version.h>

#ifndef KERNEL_VERSION
#define KERNEL_VERSION(x,y,z) (((x)<<16) + ((y)<<8) + (z))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
# include  <linux/modversions.h>
# include  <linux/wrapper.h>
# define dev_ioread32(addr)  readl(addr)
# define dev_ioread16(addr)  readw(addr)
# define dev_iowrite32(value, addr)  writel((value), (void*)(addr))
# define dev_iowrite16(value, addr)  writew((value), (void*)(addr))
#else
# define dev_ioread32(addr)  ioread32((void*)(addr))
# define dev_ioread16(addr)  ioread16((void*)(addr))
# define dev_iowrite32(value, addr)  iowrite32((value), (void*)(addr))
# define dev_iowrite16(value, addr)  iowrite16((value), (void*)(addr))
#endif
# include  <linux/pci.h>
# include  <linux/mm.h>
# include  <linux/sched.h>
# include  <linux/fs.h>
# include  <linux/signal.h>
# include  <linux/errno.h>
# include  <linux/slab.h>
# include  <linux/types.h>
# include  <linux/wait.h>
# include  <asm/segment.h>
# include  <asm/io.h>
# include  <asm/pgtable.h>
# include  <asm/uaccess.h>

struct Instance;


/* This little convenience routine returns true if the current process
   has been signalled. */

static int inline is_signalled(struct Instance*xsp)
{ return signal_pending(current); }


static inline void* allocate_real_page(void)
{ return (void*)get_zeroed_page(GFP_KERNEL|GFP_DMA32); }

static inline void free_real_page(void*ptr)
{ free_page((unsigned long)ptr); }

/*
 * $Log: os.h,v $
 * Revision 1.7  2009/02/05 17:06:57  steve
 *  Support for frame64 frame table pointers.
 *
 * Revision 1.6  2008/12/01 16:17:02  steve-icarus
 *  More robust channel table handling.
 *
 * Revision 1.5  2008/09/08 22:29:50  steve
 *  Build for 2.6 kernel
 *
 * Revision 1.4  2008/08/25 22:27:31  steve
 *  Better portability in the Linux universe.
 *
 * Revision 1.3  2002/10/21 19:04:45  steve
 *  License information for modload.
 *
 * Revision 1.2  2001/08/14 22:25:30  steve
 *  Add SseBase device
 *
 * Revision 1.1  2001/08/02 03:45:44  steve
 *  Linux 2.4 version of the driver.
 *
 */


#endif
