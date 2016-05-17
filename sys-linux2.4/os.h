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

# include  "ucrpriv.h"
struct Instance;


/* This little convenience routine returns true if the current process
   has been signalled. */

static int inline is_signalled(struct Instance*xsp)
{ return signal_pending(current); }


static inline void* allocate_real_page(struct Instance*xsp, dma_addr_t*baddr)
{
      void*addr = dma_alloc_coherent(&xsp->pci->dev, PAGE_SIZE, baddr,
				     GFP_KERNEL|GFP_DMA32);
      return addr;
}

static inline void free_real_page(struct Instance*xsp, void*ptr, dma_addr_t baddr)
{
      dma_free_coherent(&xsp->pci->dev, PAGE_SIZE, ptr, baddr);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
static inline struct inode*file_inode(struct file*filp)
{ return filp->f_dentry->d_inode; }
#endif

#endif
