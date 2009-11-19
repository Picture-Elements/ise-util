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

# include  "ucrif.h"
# include  "os.h"
# include  "ucrpriv.h"
# include  <linux/module.h>
# include  <linux/poll.h>
# include  <linux/interrupt.h>
# include  <linux/vmalloc.h>

MODULE_LICENSE("GPL");

unsigned ucr_major = 114;

int ise_limit_frame_pages = 0;

/* If page addresses may be 64bits, then enable (allow) support for
   FRAME64 frames. */
static int ise_allow_frame64 = sizeof(unsigned long) > 4;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
module_param(ucr_major, int, S_IRUGO);
module_param(ise_limit_frame_pages, int, S_IRUGO);
module_param(debug_flag, int, S_IRUGO);

# define MOD_INC_USE_COUNT try_module_get(THIS_MODULE)
# define MOD_DEC_USE_COUNT module_put(THIS_MODULE)

#else
MODULE_PARM(ucr_major,"i");
MODULE_PARM_DESC(ucr_major,"Use this major number");

MODULE_PARM(ise_limit_frame_pages,"i");
MODULE_PARM_DESC(ise_limit_frame_pages,"Limit total frame pages");

MODULE_PARM(debug_flag,"i");
MODULE_PARM_DESC(debug_flag,"Enable debug messages");

#define pci_register_driver pci_module_init
#endif

void set_time_delay(struct Instance*xsp,
		    struct timer_list*tim,
		    void (*fun)(unsigned long))
{
      init_timer(tim);
      tim->expires = jiffies + 10*HZ;
      tim->data = (unsigned long)xsp;
      tim->function = fun;
      add_timer(tim);
}

void cancel_time_delay(struct timer_list*tim)
{
      del_timer(tim);
}


#ifndef DEVICE_MAX
# define DEVICE_MAX 4
#endif


static unsigned num_dev = 0;
static int ise_frame_pages_in_use = 0;

static struct Instance inst[DEVICE_MAX];

struct Instance*ucr_find_board_instance(unsigned id)
{
      struct Instance*xsp;
      if (id >= num_dev) return 0;
      xsp = inst + id;
      return xsp;
}

static __u64 frame_page_bus(struct Instance*xsp, unsigned frame_nr, unsigned page_nr)
{
      __u64 page_bus;

      if (xsp->frame[frame_nr] == 0)
	    return 0;

      if (xsp->frame[frame_nr]->magic == FRAME_TABLE_MAGIC64) {
	      /* FRAME64 frames have addresses split into 2 32bit words. */
	    page_bus = xsp->frame[frame_nr]->page[page_nr*2+1];
	    page_bus <<= (__u64)32;
	    page_bus += (__u64) xsp->frame[frame_nr]->page[page_nr*2+0];
      } else {
	      /* FRAME32 frames are simpler. */
	    page_bus = xsp->frame[frame_nr]->page[page_nr];
      }

      return page_bus;
}

/*
 * The ucr_make_frame and ucr_free_frame functions perform the work of
 * allocating/releasing the frames. They check that the parameters are
 * reasonable, and do the allocation/release of memory. Allocate
 * PAGE_SIZE chunks because these are convenient for the mmap function.
 */

int ucr_make_frame(struct Instance*xsp, unsigned id, unsigned long size)
{
      unsigned idx;
      unsigned npages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

	/* Allocate the page pointer block, and the pages
	   themselves. If there is a problem, jump to error exit
	   code. */

      dma_addr_t tab_phys;
      unsigned long tab_size = sizeof(*xsp->frame[id]);
      int use_frame64 = dev_feature_frame64(xsp) && ise_allow_frame64;
      if (use_frame64)
	    tab_size += npages*sizeof(__u64);
      else
	    tab_size += npages*sizeof(__u32);

	/* Allocate the frame pointer table. Keep this in 32bit memory
	   even on 64bit systems. This may contain 32bit or 64bit
	   pointers, but the pointer to this table is 32bit. */
      xsp->frame[id] = (struct frame_table*)
	    dma_alloc_coherent(&xsp->pci->dev, tab_size, &tab_phys,
			       GFP_KERNEL|GFP_DMA32);
      xsp->frame_tabsize[id] = tab_size;
      
      if (xsp->frame[id] == 0) {
	    printk(KERN_INFO "ise%u: error: no memory for frame table. tab_size=%lu\n",
		   xsp->number, tab_size);
	    return -ENOMEM;
      }

      xsp->frame_virt[id] = vmalloc(npages * sizeof(void*));
      size = npages * PAGE_SIZE;

      xsp->frame[id]->magic = use_frame64? FRAME_TABLE_MAGIC64 : FRAME_TABLE_MAGIC;
      xsp->frame[id]->self  = tab_phys;
      xsp->frame[id]->page_size  = PAGE_SIZE;
      xsp->frame[id]->page_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;

      if (xsp->frame[id]->magic == FRAME_TABLE_MAGIC64) {
	    for (idx = 0 ;  idx < xsp->frame[id]->page_count ;  idx += 1) {
		  xsp->frame[id]->page[idx*2+0] = 0;
		  xsp->frame[id]->page[idx*2+1] = 0;
	    }
      } else {
	    for (idx = 0 ;  idx < xsp->frame[id]->page_count ;  idx += 1)
		  xsp->frame[id]->page[idx] = 0;
      }
      for (idx = 0 ;  idx < xsp->frame[id]->page_count ;  idx += 1)
	    xsp->frame_virt[id][idx] = 0;

      for (idx = 0 ;  idx < xsp->frame[id]->page_count ;  idx += 1) {

	    void*addr;

	    if (ise_frame_pages_in_use >= ise_limit_frame_pages) {
		  printk(KERN_INFO "ise%u: Frame failed "
			 "due to configured page limit (of %d pages).\n",
			 xsp->number, ise_limit_frame_pages);
		  goto no_mem;
	    }

	      /* Allocate an actual frame page. If the table pointer
		 is configured to support 64bit addresses, then we can
		 allocate in 64bit address space. If not, then
		 restrict to 32bit address space. */
	    if (xsp->frame[id]->magic == FRAME_TABLE_MAGIC64) {
		  dma_addr_t phys;
		  __u64 addr_bus;
		  addr = dma_alloc_coherent(&xsp->pci->dev, PAGE_SIZE, &phys, GFP_KERNEL);
		  addr_bus = phys;
		  xsp->frame[id]->page[idx*2+0] = addr_bus & 0xffffffff;
		  xsp->frame[id]->page[idx*2+1] = (addr_bus>>32) & 0xffffffff;
	    } else {
		  dma_addr_t phys;
		  addr = dma_alloc_coherent(&xsp->pci->dev, PAGE_SIZE, &phys, GFP_KERNEL|GFP_DMA32);
		  xsp->frame[id]->page[idx] = phys;
	    } 
	    if (addr == 0)
		  goto no_mem;

	    xsp->frame_virt[id][idx] = addr;
	    ise_frame_pages_in_use += 1;

	      /* Save the bus address in the frame information, and
		 mark the page reserved to keep the pager away. */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	    SetPageReserved(virt_to_page(addr));
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	    mem_map_reserve(virt_to_page(addr));
#else
	    mem_map_reserve(MAP_NR(addr));
#endif
      }


      return size;


no_mem: { /* Gack! Not enough memory, clean up what I did get and
	     return the correct error code. */
	    unsigned jdx;

	    printk("<1>ise%u: error: not enough memory for "
		   "page %u of %u frame pages.\n", xsp->number, idx, npages);

	    for (jdx = 0 ;  jdx < xsp->frame[id]->page_count ;  jdx += 1) {
		  void*virt;
		  dma_addr_t phys = frame_page_bus(xsp, id, jdx);

		  if (phys == 0)
			continue;

		  virt = xsp->frame_virt[id][jdx];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
		  ClearPageReserved(virt_to_page(virt));
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		  mem_map_unreserve(virt_to_page(virt));
#else
		  mem_map_unreserve(MAP_NR(virt));
#endif
		  dma_free_coherent(&xsp->pci->dev, PAGE_SIZE, virt, phys);

		  if (ise_frame_pages_in_use > 0)
			ise_frame_pages_in_use -= 1;
	    }

	    dma_free_coherent(&xsp->pci->dev, xsp->frame_tabsize[id], xsp->frame[id], xsp->frame[id]->self);
	    vfree(xsp->frame_virt[id]);
	    xsp->frame[id] = 0;
	    xsp->frame_virt[id] = 0;

	    return -ENOMEM;
      }
}

int ucr_free_frame(struct Instance*xsp, unsigned id)
{
      unsigned idx;
      dma_addr_t tab_phys;

      if (xsp->frame[id] == 0)
	    return -EIO;

      if (xsp->frame_ref[id] > 0)
	    return -EBUSY;

      if (debug_flag & UCR_TRACE_FRAME)
	    printk(DEVICE_NAME "%u: release pages of frame %u\n",
		   xsp->number, id);

      if (xsp->frame[id]->magic != FRAME_TABLE_MAGIC64
	  && xsp->frame[id]->magic != FRAME_TABLE_MAGIC) {
	    printk(KERN_WARNING DEVICE_NAME "%u: "
		   "Bad magic for frame %u to free! Abandoning memory.\n",
		   xsp->number, id);
	    xsp->frame[id] = 0;
	    xsp->frame_tabsize[id] = 0;
	    return 0;
      }

      for (idx = 0 ;  idx < xsp->frame[id]->page_count ;  idx += 1) {
	    dma_addr_t phys = frame_page_bus(xsp, id, idx);
	    void* virt = xsp->frame_virt[id][idx];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	    ClearPageReserved(virt_to_page(virt));
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	    mem_map_unreserve(virt_to_page(virt));
#else
	    mem_map_unreserve(MAP_NR(virt));
#endif
	    dma_free_coherent(&xsp->pci->dev, PAGE_SIZE, virt, phys);

	    if (ise_frame_pages_in_use > 0)
		  ise_frame_pages_in_use -= 1;
      }

      tab_phys = xsp->frame[id]->self;
      dma_free_coherent(&xsp->pci->dev, xsp->frame_tabsize[id], xsp->frame[id], tab_phys);
      xsp->frame[id] = 0;
      xsp->frame_tabsize[id] = 0;
      return 0;
}


static int xxopen(struct inode *inode, struct file *file)
{
      unsigned minor = MINOR(inode->i_rdev);
      int control_flag = (minor&0x80) != 0;
      struct Instance*xsp;
      int rc;

      minor &= 0x7f;

      if (minor >= num_dev) return -ENODEV;
      if (file->private_data) return -EIO;

      xsp = inst + minor;

      if (control_flag) {
	    rc =  ucrx_open(xsp);
	    if (rc != 0)
		  return rc;

      } else {

	    file->private_data = kmalloc(sizeof (struct ChannelData),
					 GFP_KERNEL);
	    if (file->private_data == 0)
		  return -ENOMEM;

	    rc = ucr_open(xsp, (struct ChannelData*)file->private_data);
	    if (rc != 0) {
		  kfree(file->private_data);
		  file->private_data = 0;
		  return rc;
	    }
      }
      MOD_INC_USE_COUNT;
      return 0;
}

int xxrelease(struct inode *inode, struct file *file)
{
      unsigned minor = 0x7f & MINOR(inode->i_rdev);
      int control_flag = (0x80 & MINOR(inode->i_rdev)) != 0;
      struct Instance*xsp = inst + minor;
      struct ChannelData*xpd = (struct ChannelData*)file->private_data;

      if (control_flag) {
	    ucrx_release(xsp);
      } else {
	    ucr_release(xsp, xpd);
	    kfree(xpd);
      }

      MOD_DEC_USE_COUNT;
      return 0;
}

static ssize_t xxread(struct file *file, char *bytes, size_t count, loff_t*off)
{
      long rc;
      unsigned minor = MINOR(file->f_dentry->d_inode->i_rdev) & 0x7f;
      int control_flag = (MINOR(file->f_dentry->d_inode->i_rdev) & 0x80) != 0;
      struct Instance*xsp = inst + minor;
      struct ChannelData*xpd = (struct ChannelData*)file->private_data;

      if (control_flag)
	    return -ENOSYS;

      rc = ucr_read(xsp, xpd, bytes, count, 
		    (file->f_flags&O_NONBLOCK)? 0 : 1);

      if (rc > 0)
	    *off += rc;

      return rc;
}

static ssize_t xxwrite(struct file*file, const char*bytes,
		       size_t count, loff_t*off)
{
      long rc;
      unsigned minor = MINOR(file->f_dentry->d_inode->i_rdev) & 0x7f;
      int control_flag = (MINOR(file->f_dentry->d_inode->i_rdev) & 0x80) != 0;
      struct Instance*xsp = inst + minor;
      struct ChannelData*xpd = (struct ChannelData*)file->private_data;

      if (control_flag)
	    return -ENOSYS;

      rc = ucr_write(xsp, xpd, bytes, count);
      if (rc > 0)
	    *off += rc;

      return rc;
}
static int xxioctl(struct inode *inode, struct file *file,
		   unsigned int cmd, unsigned long arg)
{
      unsigned minor = MINOR(inode->i_rdev) & 0x7f;
      int control_flag = (MINOR(inode->i_rdev) & 0x80) != 0;
      struct Instance*xsp = inst + minor;
      struct ChannelData*xpd = (struct ChannelData*)file->private_data;

      if (control_flag)
	    return ucrx_ioctl(xsp, cmd, arg);
      else
	    return ucr_ioctl(xsp, xpd, cmd, arg);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
typedef void irqreturn_t;
# define RETURN_IRQ_NONE do { } while (0)
# define RETURN_IRQ_HANDLED do { } while (0)
# define USE_IRQF_SHARED SA_SHIRQ
# define EXTRA_IRQ_ARGS , struct pt_regs*pr
#else
# define RETURN_IRQ_NONE return IRQ_NONE
# define RETURN_IRQ_HANDLED return IRQ_HANDLED
# define USE_IRQF_SHARED IRQF_SHARED
# define EXTRA_IRQ_ARGS
#endif
static irqreturn_t xxirq(int irq, void *dev_id EXTRA_IRQ_ARGS)
{
      struct Instance*xsp = (struct Instance*)dev_id;
      if (ucr_irq(xsp))
	    RETURN_IRQ_HANDLED;
      else
	    RETURN_IRQ_NONE;
}

/*
 * Linux 2.1 introduces an improved form of select (called poll) that
 * expects the driver to return a bitmask of all the ready events on
 * the channel. The driver is also expected to register a wait event
 * in the poll table in case the thread must wait for something
 * useful.
 */
static unsigned int xxselect(struct file*file, poll_table*pt)
{
      unsigned minor = MINOR(file->f_dentry->d_inode->i_rdev) & 0x7f;
      struct Instance*xsp = inst + minor;
      struct ChannelData*xpd = (struct ChannelData*)file->private_data;

      poll_wait(file, &xsp->dispatch_sync, pt);
      if (! CHANNEL_IN_EMPTY(xpd))
	    return POLLIN | POLLRDNORM;

      return 0;
}


static void xxvmopen(struct vm_area_struct*vma)
{
      struct inode*inode = vma->vm_file->f_dentry->d_inode;
      unsigned minor = MINOR(inode->i_rdev);
      struct Instance*xsp = inst+minor;

	/* A new reference was spontaneously created. Count it. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
      unsigned frame_nr = ((vma->vm_pgoff*PAGE_SIZE) >> 28) & 0x0f;
#else
      unsigned frame_nr = (vma->vm_offset >> 28) & 0x0f;
#endif
      MOD_INC_USE_COUNT;
      xsp->frame_ref[frame_nr] += 1;

      if (debug_flag&UCR_TRACE_FRAME)
	    printk(DEVICE_NAME "%d: vmopen\n", xsp->number);
}

static void xxvmclose(struct vm_area_struct*vma)
{
      struct inode*inode = vma->vm_file->f_dentry->d_inode;
      unsigned minor = MINOR(inode->i_rdev);
      struct Instance*xsp = inst+minor;

	/* A vma referencing the frame has disappeared. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
      unsigned frame_nr = ((vma->vm_pgoff*PAGE_SIZE) >> 28) & 0x0f;
#else
      unsigned frame_nr = (vma->vm_offset >> 28) & 0x0f;
#endif
      MOD_DEC_USE_COUNT;
      xsp->frame_ref[frame_nr] -= 1;

      if (debug_flag&UCR_TRACE_FRAME)
	    printk(DEVICE_NAME "%d: vmclose\n", xsp->number);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
/*
 * Starting around 2.6.27, the nopage function is replaced with the
 * fault function. The obvious difference is that it takes a vm_fault
 * argument, and the result is returned in vmf->page.
 */
static int xxfault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
      unsigned long offset, page_nr;
      unsigned frame_nr;
      struct inode*inode = vma->vm_file->f_dentry->d_inode;

      unsigned minor = MINOR(inode->i_rdev);
      struct Instance*xsp = inst+minor;

      unsigned long vma_offset = vma->vm_pgoff*PAGE_SIZE;

      __u64 page_bus = 0;

      frame_nr = (vma_offset >> 28) & 0x0f;
      offset = (unsigned long)vmf->virtual_address;
      offset = offset - vma->vm_start + vma_offset;
      offset &= 0x0fffffff;
      page_nr = offset / PAGE_SIZE;

      if (debug_flag&UCR_TRACE_FRAME)
	    printk(DEVICE_NAME "%d: nopage address=%p, offset=%p\n",
		   xsp->number, vmf->virtual_address, (void*)offset);

      page_bus = frame_page_bus(xsp, frame_nr, page_nr);

      vmf->page = virt_to_page(xsp->frame_virt[frame_nr][page_nr]);
      get_page(vmf->page);

      return 0;
}

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
/*
 * Version for 2.4 and early 2.6 kernels.
 */
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
typedef int* write_access_t;
# else
typedef int write_access_t;
# endif
static struct page* xxvmnopage(struct vm_area_struct*vma,
			       unsigned long address,
			       write_access_t write_access)
{
      struct page*page_out = 0;
      unsigned long offset, page_nr;
      unsigned frame_nr;
      struct inode*inode = vma->vm_file->f_dentry->d_inode;

      unsigned minor = MINOR(inode->i_rdev);
      struct Instance*xsp = inst+minor;

      unsigned long vma_offset = vma->vm_pgoff*PAGE_SIZE;

      __u64 page_bus = 0;

      frame_nr = (vma_offset >> 28) & 0x0f;
      offset = (address - vma->vm_start + vma_offset) & 0x0fffffff;
      page_nr = offset / PAGE_SIZE;

      if (debug_flag&UCR_TRACE_FRAME)
	    printk(DEVICE_NAME "%d: nopage address=%p, offset=%p\n",
		   xsp->number, (void*)address, (void*)offset);

      page_bus = frame_page_bus(xsp, frame_nr, page_nr);

      page_out = virt_to_page(bus_to_virt(page_bus));
      get_page(page_out);
      return page_out;
}
#else
/*
 * Version for old old kernels, before 2.4.
 */
static unsigned long xxvmnopage(struct vm_area_struct*vma,
				unsigned long address,
				int write_access)
{
      unsigned long offset, page_nr;
      unsigned frame_nr;
#if LINUX_VERSION_CODE > 0x020100
      struct inode*inode = vma->vm_file->f_dentry->d_inode;
#else
      struct inode*inode = vma->vm_inode;
#endif
      unsigned minor = MINOR(inode->i_rdev);
      struct Instance*xsp = inst+minor;

      frame_nr = (vma->vm_offset >> 28) & 0x0f;
      offset = (address - vma->vm_start + vma->vm_offset) & 0x0fffffff;
      page_nr = offset / PAGE_SIZE;

      if (debug_flag&UCR_TRACE_FRAME)
	    printk(DEVICE_NAME "%d: nopage address=%p, offset=%p\n",
		   xsp->number, address, offset);

      return (unsigned long)bus_to_virt(xsp->frame[frame_nr]->page[page_nr]);
}
#endif

static struct vm_operations_struct xxvm_ops = {
      open:   xxvmopen,
      close:  xxvmclose,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
      fault:  xxfault
#else
      nopage: xxvmnopage
#endif
};

/*
 * The mmap function places the 16 possible frames into a linear
 * address space with the high 4 bits used to select the frame. The
 * frame is created by the ioctl elsewhere.
 *
 * xxmmap will return an error if:
 *   The segment spans frames
 *   The segment oversteps the frame
 *
 */
static int xxmmap(struct file*file, struct vm_area_struct*vma)
{
      unsigned frame_nr;
      unsigned long offset, size, frame_size;
      unsigned minor = MINOR(file->f_dentry->d_inode->i_rdev) & 0x7f;
      struct Instance*xsp = inst+minor;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
      unsigned long vma_offset = vma->vm_pgoff * PAGE_SIZE;
#else
      unsigned long vma_offset = vma->vm_offset;
#endif

      if (debug_flag&UCR_TRACE_FRAME)
	    printk(DEVICE_NAME "%d: mmap vm_offset=%08lx, vm_start=%08lx,"
		   "vm_end=%08lx\n", xsp->number, vma_offset,
		   vma->vm_start, vma->vm_end);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
      vma->vm_flags |= VM_IO|VM_RESERVED;
#else
      vma->vm_flags |= VM_LOCKED;
#endif

      frame_nr = (vma_offset >> 28) & 0x0f;
      offset = vma_offset & 0x0fffffffU;
      size = vma->vm_end - vma->vm_start;

      if (frame_nr != (((vma_offset+size) >> 28) & 0x0f)) {
	    printk("ucr mmap: span frames %u and %lu.\n", frame_nr,
		   ((vma_offset+size) >> 28) & 0x0f);
	    return -EINVAL;
      }

      frame_size = xsp->frame[frame_nr]->page_size
	    * xsp->frame[frame_nr]->page_count;

      if ( (offset + size) > frame_size) {
	    printk("ucr mmap: size %lu too big for frame(%lu)\n",
		   (offset + size), frame_size);
	    return -EINVAL;
      }

      vma->vm_ops = &xxvm_ops;
      xsp->frame_ref[frame_nr] += 1;

      MOD_INC_USE_COUNT;

      return 0;
}

static struct file_operations ise_ops = {
      read:  xxread,
      write: xxwrite,
      poll:  xxselect,
      ioctl: xxioctl,
      mmap:  xxmmap,
      open:  xxopen,
      release: xxrelease,
};

static const struct ise_ops_tab ops_table[3] = {
      { full_name:           "Picture Elements ISE/SSE",
	feature_flags:       0,
	init_hardware:       ise_init_hardware,
	clear_hardware:      ise_clear_hardware,
	mask_irqs:           ise_mask_irqs,
	unmask_irqs:         ise_unmask_irqs,
	set_root_table_base: ise_set_root_table_base,
	set_root_table_resp: ise_set_root_table_resp,
	get_root_table_ack:  ise_get_root_table_ack,
	get_status_resp:     ise_get_status_resp,
	set_status_value:    ise_set_status_value,
	set_bells:           ise_set_bells,
	get_bells:           ise_get_bells,

	soft_reset:          ise_soft_reset,
	soft_remove:         isex_remove,
	soft_replace:        isex_replace,

	diagnose_dump:       isex_diagnose_dump
      },
      { full_name:           "Picture Elements JSE",
	feature_flags:       0,
	init_hardware:       jse_init_hardware,
	clear_hardware:      jse_clear_hardware,
	mask_irqs:           jse_mask_irqs,
	unmask_irqs:         jse_unmask_irqs,
	set_root_table_base: jse_set_root_table_base,
	set_root_table_resp: jse_set_root_table_resp,
	get_root_table_ack:  jse_get_root_table_ack,
	get_status_resp:     jse_get_status_resp,
	set_status_value:    jse_set_status_value,
	set_bells:           jse_set_bells,
	get_bells:           jse_get_bells,

	soft_reset:          jse_soft_reset,
	soft_remove:         jsex_remove,
	soft_replace:        jsex_replace,

	diagnose_dump:       jsex_diagnose_dump
      },
      { full_name:           "Picture Elements EJSE",
	feature_flags:       ISE_FEATURE_FRAME64,
	init_hardware:       ejse_init_hardware,
	clear_hardware:      ejse_clear_hardware,
	mask_irqs:           ejse_mask_irqs,
	unmask_irqs:         ejse_unmask_irqs,
	set_root_table_base: ejse_set_root_table_base,
	set_root_table_resp: ejse_set_root_table_resp,
	get_root_table_ack:  ejse_get_root_table_ack,
	get_status_resp:     ejse_get_status_resp,
	set_status_value:    ejse_set_status_value,
	set_bells:           ejse_set_bells,
	get_bells:           ejse_get_bells,

	soft_reset:          ejse_soft_reset,
	soft_remove:         ejsex_remove,
	soft_replace:        ejsex_replace,

	diagnose_dump:       ejsex_diagnose_dump
      }
};

static int ise_probe(struct pci_dev*dev, const struct pci_device_id*id)
{
      unsigned long base, len;
      struct Instance*xsp = inst + num_dev;
      int rc;

      if (num_dev >= DEVICE_MAX) {
	    printk(KERN_WARNING "ise: Too many devices "
		   "(>%u) for driver.\n", DEVICE_MAX);
	    return -EIO;
      }

	/* Select board manipulation functions. */
      switch (id->driver_data) {

	  case 0:
	    xsp->dev_ops = ops_table+0;
	    break;

	  case 1:
	    xsp->dev_ops = ops_table+1;
	    break;

	  case 2:
	    xsp->dev_ops = ops_table+2;
	    break;

	  default:
	    printk(KERN_WARNING "ise: bad device_id record?\n");
	    return -EIO;
      }

      xsp->number = num_dev;
      xsp->pci = dev;
      ucr_init_instance(xsp);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
      dev->dev.driver_data = xsp;
#else
      dev->driver_data = xsp;
#endif
      pci_enable_device(xsp->pci);

      printk(KERN_INFO DEVICE_NAME "%u: Found device at BAR0=0x%lx-0x%lx IRQ=%u\n",
	     num_dev, (unsigned long)pci_resource_start(dev, 0),
	     (unsigned long)pci_resource_end(dev, 0), xsp->pci->irq);

	/* Get the address of the DEVICE_NAME registers. Make sure the
	   memory is mapped into the kernel address space. By
	   default, it isn't necessarily. */

      base = pci_resource_start(xsp->pci, 0);
      len = pci_resource_end(xsp->pci, 0) - base + 1;

	/* XXXX xsp->reg = request_mem_region(base, len, DEVICE_NAME); */
      xsp->dev = (unsigned long)ioremap_nocache(base, len);

	/* Get the interrupt number from the PCI config space,
	   and register an interrupt handler to deal with it. */
      rc = request_irq(xsp->pci->irq, xxirq, USE_IRQF_SHARED, DEVICE_NAME, xsp);
      if (rc < 0) {
	printk(KERN_INFO DEVICE_NAME "%u: Unable to request IRQ=%u\n",
	       num_dev, xsp->pci->irq);
      }

      dev_init_hardware(xsp);

      printk(DEVICE_NAME "%u is a %s.\n", num_dev, xsp->dev_ops->full_name);
      isecons_log("ise%u: found device.\n", num_dev);
      num_dev += 1;

      return 0;
}

static void ise_remove(struct pci_dev*dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
      struct Instance*xsp = dev->dev.driver_data;
#else
      struct Instance*xsp = dev->driver_data;
#endif

      dev_clear_hardware(xsp);
      free_irq(xsp->pci->irq, xsp);

	/*pci_disable_device(xsp->pci);*/
      ucr_clear_instance(xsp);
}

static const struct pci_device_id ise_idtable [] = {
      { 0x12c5, 0x007f, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
      { 0x8086, 0xb555,     0x12c5,     0x008a, 0, 0, 1},
      { 0x12c5, 0x0091, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 2},
      { 0, 0, 0, 0, 0, 0, 0}
};

static struct pci_driver ise_driver = {
      name:     DEVICE_NAME,
      id_table: ise_idtable,
      probe:    ise_probe,
      remove:   ise_remove
};

static int __init init_ise_module(void)
{
      int rc;

      if (ise_limit_frame_pages == 0)
	    ise_limit_frame_pages = num_physpages / 2;

      isecons_init();

      printk(DEVICE_NAME ": Limit frames to total of %d pages\n",
	     ise_limit_frame_pages);

      if (debug_flag)
	    printk(DEVICE_NAME ": Debug_flag set to %u\n", debug_flag);

      rc = register_chrdev(ucr_major, DEVICE_NAME, &ise_ops);
      if (rc < 0) {
	    printk(KERN_INFO DEVICE_NAME ": Unable to register char dev %u\n", ucr_major);
	    return rc;
      }

      rc = pci_register_driver(&ise_driver);
      if (rc < 0) {
	    printk(KERN_INFO DEVICE_NAME ": Error %d initializing pci nodule\n", -rc);
	    unregister_chrdev(ucr_major, DEVICE_NAME);
      }

      return rc;
}

static void __exit cleanup_ise_module(void)
{
      pci_unregister_driver(&ise_driver);
      unregister_chrdev(ucr_major, DEVICE_NAME);
      isecons_release();
}


module_init(init_ise_module);
module_exit(cleanup_ise_module);
