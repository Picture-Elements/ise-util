/*
 * Copyright (c) 1998-2002 Picture Elements, Inc.
 *    Stephen Williams <steve@picturel.com>
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
 *
 *    You should also have recieved a copy of the Picture Elements
 *    Binary Software License offer along with the source. This offer
 *    allows you to obtain the right to redistribute the software in
 *    binary (compiled) form. If you have not received it, contact
 *    Picture Elements, Inc., 777 Panoramic Way, Berkeley, CA 94704.
 */

/*
 * See README.txt for a detailed description of how the interaction
 * with the board works.
 */

# include  "ucrif.h"

/*
 */
# include  "os.h"
# include  "ucrpriv.h"

# define ROOT_TABLE_BELLMASK 0x01
# define STATUS_BELLMASK     0x02
# define CHANGE_BELLMASK     0x04


unsigned debug_flag = 0;

/*
 * Construct a fresh instance structure. This does not depend on the
 * initial zero value that the C compiler would assign.
 */
void ucr_init_instance(struct Instance*xsp)
{
      dma_addr_t baddr;
      unsigned idx;
      xsp->dev = 0;
      xsp->suspense = 0;
      xsp->channels = 0;

      xsp->root = (struct root_table*)allocate_real_page(xsp, &baddr);
      init_waitqueue_head(&xsp->root_sync);
      init_waitqueue_head(&xsp->dispatch_sync);

      xsp->root->magic = ROOT_TABLE_MAGIC;
      xsp->root->self  = baddr;

      for (idx = 0 ;  idx < 16 ;  idx += 1) {
	    xsp->root->frame_table[idx].ptr = 0;
	    xsp->root->frame_table[idx].magic = 0;
	    xsp->frame[idx] = 0;
	    xsp->frame_ref[idx] = 0;
	    xsp->frame_virt[idx] = 0;
      }

      for (idx = 0 ;  idx < ROOT_TABLE_CHANNELS ;  idx += 1) {
	    xsp->root->chan[idx].ptr = 0;
	    xsp->root->chan[idx].magic = 0;
      }

      xsp->channel_table_pool = (struct channel_table*)allocate_real_page(xsp, &baddr);
      for (idx = 0 ; idx < (PAGE_SIZE/sizeof(struct channel_table)) ; idx += 1) {
	    xsp->channel_table_pool[idx].magic = 0;
	    xsp->channel_table_pool[idx].self = 0;
      }
      xsp->channel_table_phys = baddr;
}

void ucr_clear_instance(struct Instance*xsp)
{
      unsigned idx;
      for (idx = 0 ;  idx < 16 ;  idx += 1)
	    if (xsp->frame[idx])
		  ucr_free_frame(xsp, idx);

      free_real_page(xsp, xsp->channel_table_pool, xsp->channel_table_phys);
}

static struct channel_table*allocate_channel_table(struct Instance*xsp)
{
      unsigned idx;
      for (idx = 0 ; idx < (PAGE_SIZE/sizeof(struct channel_table)) ; idx += 1) {
	    if (xsp->channel_table_pool[idx].magic == 0) {
		  xsp->channel_table_pool[idx].magic = CHANNEL_TABLE_MAGIC;
		  xsp->channel_table_pool[idx].self  = xsp->channel_table_phys + idx*sizeof(struct channel_table);
		    /* Spare copies of magic numbers.... */
		  xsp->channel_table_pool[idx].frame = xsp->channel_table_phys + idx*sizeof(struct channel_table);
		  xsp->channel_table_pool[idx].reserved = CHANNEL_TABLE_MAGIC;

		  return xsp->channel_table_pool + idx;
	    }
      }
      return 0;
}

static void free_channel_table(struct Instance*xsp, volatile struct channel_table*table)
{
      long idx;
      if (table->magic != CHANNEL_TABLE_MAGIC)
	    return;

      idx = table - xsp->channel_table_pool;
      if (idx < 0 || idx >= (PAGE_SIZE/sizeof(struct channel_table)))
	    return;

      table->self -= idx * sizeof(struct channel_table);
      if (table->self != xsp->channel_table_phys)
	    return;

      table->magic = 0;
      table->self = 0;
      table->reserved = 0;
}

/*
 * This function finds the ChannelData structure that has the
 * specified id. If no structures match, return 0. The result comes
 * from the instance structure, which has a table of opened channels.
 */
struct ChannelData* channel_by_id(struct Instance*xsp, unsigned short id)
{
      struct ChannelData*cp;

      if (xsp->channels == 0) return 0;
      cp = xsp->channels;

      do {
	    cp = cp->next;
	    if (cp->channel == id) return cp;
      } while (cp != xsp->channels);

      return 0;
}

/*
 * This function updates the root table by writing to the root table
 * base pointer on the ISE board. The set also clears the OMR0
 * register so that I notice when the ISE board writes my address back
 * into it.
 *
 * If the root pointer is zero(0), the operation performs a disconnect
 * instead, which involves writing a zero to the root table pointer
 * without clearing the resp pointer.
 */
static void root_timeout(unsigned long cd)
{
      struct Instance*xsp = (struct Instance*)cd;
      xsp->root_timeout_flag = 1;
      wake_up(&xsp->root_sync);
}

static int root_to_board(struct Instance*xsp, __u32 root)
{
      wait_queue_t wait;
      unsigned long mask;
      struct timer_list root_timer;

      if (debug_flag & UCR_TRACE_PROTO)
	    printk(DEVICE_NAME "%u: root to target board, "
		   "root table=%x xsp->dev=%lx.\n", xsp->number, root, xsp->dev);


      mask = dev_mask_irqs(xsp);
      if (root) dev_set_root_table_resp(xsp, 0);
      dev_set_root_table_base(xsp, root);
      dev_set_bells(xsp, ROOT_TABLE_BELLMASK);

      xsp->root_timeout_flag = 0;
      set_time_delay(xsp, &root_timer, root_timeout);

	/* Wait for the target board to respond, or timeout to
	   expire. I will know that what I desire has happened by the
	   contents of the root_table_ack register. */
      init_waitqueue_entry(&wait, current);
      add_wait_queue(&xsp->root_sync, &wait);
      while (1) {
	    set_current_state(TASK_INTERRUPTIBLE);
	    if (dev_get_root_table_ack(xsp) == root) {
		  if (xsp->root_timeout_flag)
			printk(DEVICE_NAME "%u: Root table received, "
			       "but IRQ response lost.\n", xsp->number);
		  break;
	    }
	    if (xsp->root_timeout_flag) {
		  printk(DEVICE_NAME "%u: timeout sending "
			 "root table.\n", xsp->number);
		  break;
	    }
	    dev_unmask_irqs(xsp, mask);
	    schedule();
	    mask = dev_mask_irqs(xsp);
      }
      set_current_state(TASK_RUNNING);
      remove_wait_queue(&xsp->root_sync, &wait);
      dev_unmask_irqs(xsp, mask);

      cancel_time_delay(&root_timer);

      return 0;
}

/*
 * These function manage the allocation and release of root tables. I
 * need to create a duplicate (with magic numbers correct) in order to
 * make changes, and I need to be able to release tables.
 */
static struct root_table* duplicate_root(struct Instance*xsp)
{
      dma_addr_t phys;
      struct root_table*rp = allocate_real_page(xsp, &phys);
      if (rp == 0) {
	    printk(DEVICE_NAME "%u: ERROR ALLOCATING A ROOT PAGE.\n",
		   xsp->number);
	    return 0;
      }
      memcpy(rp, xsp->root, sizeof*rp);
      rp->self = phys;
      return rp;
}

static void release_root(struct Instance*xsp, struct root_table*rp)
{
      dma_addr_t phys = rp->self;
      rp->magic = 0x11111111;
      rp->self = 0;
      free_real_page(xsp, rp, phys);
}

static int root_to_board_free(struct Instance*xsp, struct root_table*rp)
{
      int rc;

      rc = root_to_board(xsp, rp->self);
      release_root(xsp, xsp->root);
      xsp->root = rp;
      return rc;
}

/*
 * this function is called to cause the channel to be switched to a
 * new id. This means moving the table to a new place in the root
 * table, so the root table must be updated.
 */
static int switch_channel(struct Instance*xsp,
			  struct ChannelData*xpd,
			  unsigned newid)
{
      struct root_table*rt = duplicate_root(xsp);
      if (rt == 0)
	    return -ENOMEM;

      rt->chan[newid].ptr   = rt->chan[xpd->channel].ptr;
      rt->chan[newid].magic = rt->chan[xpd->channel].magic;

      rt->chan[xpd->channel].ptr = 0;
      rt->chan[xpd->channel].magic = 0;

      xpd->channel = newid;

      root_to_board_free(xsp, rt);

      return 0;
}

static long make_and_set_frame(struct Instance*xsp, unsigned id,
			       unsigned long size)
{
      long asize;
      struct root_table*newroot;

      if (xsp->frame[id]) {
	    asize = xsp->frame[id]->page_size * xsp->frame[id]->page_count;

	    if (debug_flag & UCR_TRACE_FRAME)
		  printk(DEVICE_NAME "%u: recycle frame %u "
			 "final size=%lu bytes\n", xsp->number, id, asize);

	    return asize;
      }


      if (debug_flag & UCR_TRACE_FRAME)
	    printk(DEVICE_NAME "%u: Make frame %u size=%lu bytes\n",
		   xsp->number, id, size);

      asize = ucr_make_frame(xsp, id, size);
      if (asize < 0) {
	    if (debug_flag & UCR_TRACE_FRAME)
		  printk(DEVICE_NAME "%u: Failed (%ld) to make frame\n",
			 xsp->number, asize);
	    return asize;
      }

      newroot = duplicate_root(xsp);
      if (newroot == 0) {
	    if (debug_flag & UCR_TRACE_FRAME)
		  printk(DEVICE_NAME "%u: Failed to allocate duplicate root\n",
			 xsp->number);
	    ucr_free_frame(xsp, id);
	    return -ENOMEM;
      }

      newroot->frame_table[id].ptr   = xsp->frame[id]->self;
      newroot->frame_table[id].magic = xsp->frame[id]->magic;
      root_to_board_free(xsp, newroot);

      if (debug_flag & UCR_TRACE_FRAME)
	    printk(DEVICE_NAME "%u: frame %u final size=%lu bytes\n",
		   xsp->number, id, asize);

      return asize;
}

static int free_and_set_frame(struct Instance*xsp, unsigned id)
{
      struct root_table*newroot;

      if (!xsp->frame[id])
	    return -EIO;

      if (debug_flag & UCR_TRACE_FRAME)
	    printk(DEVICE_NAME "%u: Free frame %u\n", xsp->number, id);

	/* First tell the ISE board that the frame is gone. */
      if (xsp->root) {
	    newroot = duplicate_root(xsp);
	    newroot->frame_table[id].ptr = 0;
	    newroot->frame_table[id].magic = 0;
	    root_to_board_free(xsp, newroot);
      }

	/* Finally, make the frame really be gone. */
      return ucr_free_frame(xsp, id);
}

static int wait_for_write_ring(struct Instance*xsp, struct ChannelData*xpd)
{
      wait_queue_t wait_cell;

      if (NEXT_OUT_IDX(xpd->table->next_out_idx) != xpd->table->first_out_idx)
	    return 0;

      init_waitqueue_entry(&wait_cell, current);
      add_wait_queue(&xsp->dispatch_sync, &wait_cell);
      while ((NEXT_OUT_IDX(xpd->table->next_out_idx)
		    == xpd->table->first_out_idx)
	     && !signal_pending(current)) {

	    set_current_state(TASK_INTERRUPTIBLE);
	    if (debug_flag & UCR_TRACE_CHAN)
		  printk(DEVICE_NAME "%u.%u (d): wait for space"
			 " in write ring.\n", xsp->number,
			 xpd->channel);

	    schedule();
      }
      set_current_state(TASK_RUNNING);
      remove_wait_queue(&xsp->dispatch_sync, &wait_cell);

      if (signal_pending(current)) {
	    printk(DEVICE_NAME "%u.%u (d): Interrupted wait "
		   "for space in write ring.\n", xsp->number,
		   xpd->channel);
	    return -ERESTARTSYS;
      }

      return 0;
}


/*
 * When the generic part of the ucr driver finds that it must block
 * waiting for more data, it calls this function. It returns -EINTR if
 * the operation is marked pending (and the read should abort).
 */

static void read_timeout(unsigned long cd)
{
      struct ChannelData*xpd = (struct ChannelData*)cd;
      struct Instance*xsp = xpd->xsp;

      xpd->read_timeout_flag = 1;
      xpd->read_timing = 0;
      wake_up(&xsp->dispatch_sync);
}

static int wait_for_read_data(struct Instance*xsp, struct ChannelData*xpd)
{
      wait_queue_t wait_cell;

      init_waitqueue_entry(&wait_cell, current);
      add_wait_queue(&xsp->dispatch_sync, &wait_cell);

      if (xpd->read_timeout > 0) {
	    xpd->read_timing = 1;
	    init_timer(&xpd->read_timer);
	    xpd->read_timer.data = (unsigned long)xpd;
	    xpd->read_timer.expires = jiffies + xpd->read_timeout;
	    xpd->read_timer.function = &read_timeout;
	    add_timer(&xpd->read_timer);
      }

      while (CHANNEL_IN_EMPTY(xpd)
	     && ! signal_pending(current)
	     && ! xpd->read_timeout_flag) {

	    set_current_state(TASK_INTERRUPTIBLE);
	    if (debug_flag & UCR_TRACE_CHAN)
		  printk(DEVICE_NAME "%u.%u (d): wait for data"
			 " in read ring\n",
			 xsp->number, xpd->channel);

	    schedule();
      }

      set_current_state(TASK_RUNNING);
      remove_wait_queue(&xsp->dispatch_sync, &wait_cell);

      if (xpd->read_timing) {
	    del_timer(&xpd->read_timer);
	    xpd->read_timing = 0;
      }

      if (signal_pending(current)) {
	    printk(DEVICE_NAME "%u.%u: Interrupted wait "
		   "for read data.\n", xsp->number, xpd->channel);
	    return -ERESTARTSYS;
      }

      return 0;
}

/*
 * Flushing a channel causes the pending write buffer to be marked as
 * ready to be sent to the target board. This only blocks on the lack
 * of space on the buffer ring, and does not guarantee that the data
 * has been read by the target board.
 */
static int flush_channel(struct Instance*xsp, struct ChannelData*xpd)
{
      int rc;

      if (xpd->out_off == 0)
	    return 0;

      rc = wait_for_write_ring(xsp, xpd);
      if (rc < 0)
	    return rc;

	/* Set the count for the buffer that I am working on. */
      xpd->table->out[xpd->table->next_out_idx].count = xpd->out_off;

	/* Initialize the count of the next buffer that I am going to
	   be workin on shortly, and clear my offset pointer. */
      rc = NEXT_OUT_IDX(xpd->table->next_out_idx);
      xpd->table->out[rc].count = PAGE_SIZE;
      xpd->out_off = 0;

	/* Tell the target board that the next_out pointer has
	   moved. This causes the board to notice that the buffer is
	   ready. */
      xpd->table->next_out_idx = rc;
      dev_set_bells(xsp, CHANGE_BELLMASK);

      return 0;
}

static int file_mark_channel(struct Instance*xsp, struct ChannelData*xpd)
{
      int rc;
      rc = flush_channel(xsp, xpd);
      if (rc == -EINTR)
	    return rc;

      rc = wait_for_write_ring(xsp, xpd);
      if (rc < 0)
	    return rc;

	/* This sends a file mark by writing a 0 count. The target
	   understands that 0 count to be a file mark. */

      xpd->table->out[xpd->table->next_out_idx].count = 0;
      rc = NEXT_OUT_IDX(xpd->table->next_out_idx);
      xpd->table->out[rc].count = PAGE_SIZE;
      xpd->out_off = 0;

      xpd->table->next_out_idx = rc;
      dev_set_bells(xsp, CHANGE_BELLMASK);
      return 0;
}

/*
 * This function doesn't change what data is pending, it just waits
 * until that data has been consumed by the board. It is useful, for
 * example, when waiting for pending channel to be written before
 * closing that channel.
 */
static int sync_channel(struct Instance*xsp, struct ChannelData*xpd)
{
      unsigned long mask = dev_mask_irqs(xsp);

      wait_queue_t wait;
      init_waitqueue_entry(&wait, current);

      add_wait_queue(&xsp->dispatch_sync, &wait);
      while (1) {
	    set_current_state(TASK_INTERRUPTIBLE);
	    if (xpd->table->first_out_idx == xpd->table->next_out_idx)
		  break;
	    if (signal_pending(current))
		  break;
	    if (debug_flag & UCR_TRACE_CHAN)
		  printk(DEVICE_NAME "%u.%u (d): wait for data"
			 " in write ring to be consumed.\n",
			 xsp->number, xpd->channel);

	    dev_unmask_irqs(xsp, mask);
	    schedule();
	    mask = dev_mask_irqs(xsp);
      }
      set_current_state(TASK_RUNNING);
      remove_wait_queue(&xsp->dispatch_sync, &wait);

      dev_unmask_irqs(xsp, mask);

      if (signal_pending(current))
	    return -EINTR;
      else
	    return 0;
}

static long do_sync(struct Instance*xsp, struct ChannelData*xpd)
{
      if (debug_flag & UCR_TRACE_CHAN)
	    printk(DEVICE_NAME "%u.%u (d): Request sync.\n",
		   xsp->number, xpd->channel);

      if (flush_channel(xsp, xpd) == -EINTR)
	    return -EINTR;

      if (sync_channel(xsp, xpd) == -EINTR)
	    return -EINTR;

      return 0;
}

/*
 * When a new file is opened, create a ChannelData structure to be
 * associated with that descriptor. The file by default gets channel
 * 0, but the application must somehow assure that there are no two
 * files opened to the same channel, once real I/O occurs.
 *
 * If no other processes have this device open, then try to connect to
 * the ISE board by delivering the root table.
 */
int ucr_open(struct Instance*xsp, struct ChannelData*xpd)
{
      unsigned idx;
      struct root_table*newroot;

      if (xsp->suspense) {
	    printk("<1>ise%u: attempt to open suspended board.\n",
		   xsp->number);
	    return -EBUSY;
      }

      if (xsp->channels == 0) {
	    int rc = root_to_board(xsp, xsp->root->self);
	    if (rc < 0)
		  return rc;
      }

	/* Prevent a duplicate open of channel 0. */
      if (channel_by_id(xsp, 0))
	    return -EBUSY;

      xpd->channel = 0;
      xpd->xsp = xsp;
      xpd->in_off = 0;
      xpd->out_off = 0;
      xpd->read_timeout = UCRX_TIMEOUT_OFF;
      xpd->read_timing = 0;
      init_timer(&xpd->read_timer);

      xpd->table = allocate_channel_table(xsp);
      xpd->table->first_out_idx = 0;
      xpd->table->next_out_idx  = 0;
      xpd->table->first_in_idx  = 0;
      xpd->table->next_in_idx   = 0;

      for (idx = 0 ;  idx < CHANNEL_IBUFS ;  idx += 1) {
	    dma_addr_t phys;
	    xpd->in[idx] = allocate_real_page(xsp, &phys);
	    xpd->table->in[idx].ptr = phys;
	    xpd->table->in[idx].count = PAGE_SIZE;
      }

      for (idx = 0 ;  idx < CHANNEL_OBUFS ;  idx += 1) {
	    dma_addr_t phys;
	    xpd->out[idx] = allocate_real_page(xsp, &phys);
	    xpd->table->out[idx].ptr = phys;
	    xpd->table->out[idx].count = PAGE_SIZE;
      }


	/* Put the channel information into the channel list so that
	   arriving packets can be properly dispatched to the
	   process. */

      if (xsp->channels == 0) {
	    xsp->channels = xpd;
	    xpd->next = xpd;
	    xpd->prev = xpd;
      } else {
	    xpd->next = xsp->channels;
	    xpd->prev = xsp->channels->prev;
	    xpd->next->prev = xpd;
	    xpd->prev->next = xpd;
      }

	/* Update the root table to contain the new channel. This
	   involves some communication with the target board, so there
	   is opportunity for blocking here. */
      newroot = duplicate_root(xsp);
      newroot->chan[0].magic = xpd->table->magic;
      newroot->chan[0].ptr   = xpd->table->self;
      root_to_board_free(xsp, newroot);

      return 0;
}

/*
 * Release the file. Take the ChannelData structure out of the channel
 * list and return all the unread buffers.
 */

void ucr_release(struct Instance*xsp, struct ChannelData*xpd)
{
      unsigned idx;
      struct root_table*newroot;


	/* Remove the channel from the root table that the ISE board
	   is using. Do this early so that I am free to clean up the
	   channel table afterwords. */

      newroot = duplicate_root(xsp);
      newroot->chan[xpd->channel].magic = 0;
      newroot->chan[xpd->channel].ptr = 0;
      root_to_board_free(xsp, newroot);

      if (debug_flag&UCR_TRACE_CHAN)
	    printk(DEVICE_NAME "%u.%u (d): releasing buffers\n",
		   xsp->number, xpd->channel);

      for (idx = 0 ;  idx < CHANNEL_IBUFS ;  idx += 1) {
	    free_page((unsigned long)xpd->in[idx]);
      }

      for (idx = 0 ;  idx < CHANNEL_OBUFS ;  idx += 1) {
	    free_page((unsigned long)xpd->out[idx]);
      }


	/* No more communication with the target channel, so remove
	   the ChannelData structure from the channel list, remove
	   buffers, and release the xpd object. */

      if (xsp->channels == xpd)
	    xsp->channels = xsp->channels->next;
      if (xsp->channels == xpd)
	    xsp->channels = 0;
      else {
	    xpd->prev->next = xpd->next;
	    xpd->next->prev = xpd->prev;
      }

      free_channel_table(xsp, xpd->table);

      if (debug_flag&UCR_TRACE_CHAN)
	    printk(DEVICE_NAME "%u.%u (d): channel closed.\n",
		   xsp->number, xpd->channel);

      if (xsp->channels == 0)
	    root_to_board(xsp, 0);
}

/*
 * Read works by waiting for at least one packet to be dispatched to
 * the channel. I then copy the bytes out of the packet into the
 * reader's buffer, and as I drain packets return them to the read
 * pool.
 *
 * If the block_flag is true, then this read will block until at least
 * some bytes are transferred. Otherwise, do not block.
 */
long ucr_read(struct Instance*xsp, struct ChannelData*xpd,
	      char *bytes, unsigned long count, int block_flag)
{
      unsigned tcount = count;

      if (debug_flag & UCR_TRACE_CHAN)
	    printk(DEVICE_NAME
		   "%u.%u (d): ucr_read %lu bytes, block=%s\n",
		   xsp->number, xpd->channel, count,
		   block_flag?"true":"false");


      while (tcount > 0) {

	    unsigned trans, siz;
	    void*buf;

	    if (CHANNEL_IN_EMPTY(xpd)) {

		    /* If I transferred at least a few bytes to the
		       caller, then that is good enough. Don't block. */
		  if (tcount < count)
			break;

		    /* I'm about to block. If the caller doesn't want
		       that, then break from the read loop here. */
		  if (!block_flag)
			break;

		    /* It is prudent before blocking on a read to
		       flush the write buffer. I may be blocking on a
		       response to something in the write buffer! */
		  if (flush_channel(xsp, xpd) < 0)
			goto signalled;

		    /* Finally, block. If I exit and an interrupted,
		       jump out of the read. Note that this function
		       will recheck for the presence of read data in
		       an interrupt safe way. */
		  xpd->read_timeout_flag = 0;
		  if (wait_for_read_data(xsp, xpd) < 0)
			goto signalled;

		    /* Check for a timeout. This can happen if the
		       UCRX_TIMEOUT_FORCE ioctl is invoked. */
		  if (xpd->read_timeout_flag)
			goto read_timeout;
	    }

	    buf = xpd->in[xpd->table->first_in_idx];
	    siz = xpd->table->in[xpd->table->first_in_idx].count;

	    trans = tcount;
	    if ((trans + xpd->in_off) > siz)
		  trans = siz - xpd->in_off;

	    if (debug_flag & UCR_TRACE_CHAN)
		  printk(DEVICE_NAME
			 "%u.%u (d): read %u of %u bytes "
			 "[in_off=%u]\n", xsp->number,
			 xpd->channel, trans, siz, xpd->in_off);

	    if (copy_to_user(bytes, (char*)buf + xpd->in_off, trans) != 0)
		  printk(DEVICE_NAME "%u.%u: copy_to_user failed reading %u bytes?\n", xsp->number, xpd->channel, trans);

	    tcount -= trans;
	    bytes += trans;
	    xpd->in_off += trans;

	      /* If I get to the end of a buffer, release the current
		 one and notify the ISE board that the channel has
		 changed. This may get me more read buffers. */
	    if (xpd->in_off == siz) {
		  xpd->in_off = 0;
		  xpd->table->in[xpd->table->first_in_idx].count = PAGE_SIZE;
		  INCR_IN_IDX(xpd->table->first_in_idx);
		  dev_set_bells(xsp, CHANGE_BELLMASK);
	    }
      }

 read_timeout:
	/* Calculate the return value. */
      count = count - tcount;

      if (debug_flag & UCR_TRACE_CHAN)
	    printk(DEVICE_NAME "%u.%u (d): ucr_read complete %ld bytes\n",
		   xsp->number, xpd->channel, count);


      return count;

	/* Handle being interrupted (or on NT being marked pending)
	   with no bytes transferred. If any bytes were returned, even
	   if not enough, then the read will return partial results
	   instead of this error. */
signalled:
      if (debug_flag & UCR_TRACE_CHAN)
	    printk(DEVICE_NAME "%u.%u (d): ucr_read interrupted "
		   "after %lu bytes\n", xsp->number, xpd->channel,
		   count - tcount);

      return -ERESTARTSYS;
}


/*
 * The write works by waiting for the previous DMA write to finish,
 * then starting a new one for this data. The DMA operation may
 * proceed without me waiting around in the write, but I can't start a
 * new one until the DMA completes. For this to work, the mark bit 0
 * must start out as 1.
 */

long ucr_write(struct Instance*xsp, struct ChannelData*xpd,
	       const char*bytes, const unsigned long count)
{
      unsigned tcount = count;

      if (debug_flag & UCR_TRACE_CHAN)
	    printk(DEVICE_NAME "%u.%u (d): ucr_write %lu bytes\n",
		   xsp->number, xpd->channel, count);

      while (tcount > 0) {
	    unsigned trans = tcount;
	    void*buf;
	    unsigned idx;

	    idx = xpd->table->next_out_idx;
	    buf = xpd->out[idx];

	    if ((xpd->out_off + trans) > xpd->table->out[idx].count)
		  trans = xpd->table->out[idx].count - xpd->out_off;

	    if (debug_flag & UCR_TRACE_CHAN)
		  printk(DEVICE_NAME "%u.%u (d): write %u bytes to out_ptr=%u"
			 "(buf=%p) offset=%u\n", xsp->number,
			 xpd->channel, trans, idx, buf, xpd->out_off);

	    copy_from_user((char*)buf + xpd->out_off, bytes, trans);

	    xpd->out_off += trans;
	    tcount -= trans;
	    bytes  += trans;

	      /* If the current buffer is full, then send it to the
		 ISE board for reading. Block only if I am so far
		 ahead that there is no place in the circular buffer
		 for this message. */
	    if (xpd->out_off == xpd->table->out[idx].count) {

		  if (flush_channel(xsp, xpd) < 0)
			goto signalled;
	    }
      }


      if (debug_flag & UCR_TRACE_CHAN)
	    printk(DEVICE_NAME "%u.%u (d): ucr_write complete %ld bytes\n",
		   xsp->number, xpd->channel, count);

      return count;


signalled:
	/* If waiting for buffer space for the output is interrupted
	   by a signal, exit here. It is also possible that the block
	   in the flush_channel was interrupted, but that is OK. The
	   data that I wrote is written, and a later write or UCR_FLUSH
	   will complete the flush_channel. At any rate, the data *is*
	   written so include it in the write count.

	   XXXX What if there are no writes or flushes after this?
	   There should be because the caller got a partial read
	   (otherwise there would be no flush) but a broken app? XXXX */

      if (debug_flag & UCR_TRACE_CHAN)
	    printk(DEVICE_NAME "%u.%u (d): ucr_write interrupted\n",
		   xsp->number, xpd->channel);

      if (count == tcount) return -EINTR;
      else return count - tcount;
}

int ucr_ioctl(struct Instance*xsp, struct ChannelData*xpd,
	      unsigned int cmd, unsigned long arg)
{

      switch (cmd) {

	  case UCR_FLUSH:
	    if (debug_flag & UCR_TRACE_CHAN)
		  printk(DEVICE_NAME "%u.%u (d): Request flush.\n",
			 xsp->number, xpd->channel);

	    return flush_channel(xsp, xpd);

	  case UCR_SYNC:
	    return do_sync(xsp, xpd);

	  case UCR_CHANNEL:
	    if (debug_flag & UCR_TRACE_CHAN)
		  printk(DEVICE_NAME "%u.%u (d): switch to channel %lu\n",
			 xsp->number, xpd->channel, arg);

	    if (arg >= ROOT_TABLE_CHANNELS) return -EINVAL;
	    if (xpd->channel == arg) return 0;
	    if (channel_by_id(xsp, (__u16)arg)) return -EBUSY;
	    flush_channel(xsp, xpd);
	    sync_channel(xsp, xpd);
	    switch_channel(xsp, xpd, arg);

	    if (debug_flag & UCR_TRACE_CHAN)
		  printk(DEVICE_NAME "%u.%u (d): switch complete\n",
			 xsp->number, xpd->channel);

	    return 0;

	  case UCR_ATTENTION:
	    if (debug_flag & UCR_TRACE_CHAN)
		  printk(DEVICE_NAME "%u.%u (d): software attention %lu\n",
			 xsp->number, xpd->channel, arg);

	    printk("XXXX Attention not implemented\n");
	    return -ENOSYS;

	  case UCR_SEND_FILE_MARK:
	    if (debug_flag & UCR_TRACE_CHAN)
		  printk(DEVICE_NAME "%u.%u (d): send file mark\n",
			 xsp->number, xpd->channel);

	    return file_mark_channel(xsp, xpd);

	  case UCR_MAKE_FRAME: {
		unsigned id = 0xf & (arg >> 28);
		return make_and_set_frame(xsp, id, arg&0x0fffffff);
	  }

	  case UCR_FREE_FRAME: {
		unsigned id = 0xf & (arg >> 28);
		return free_and_set_frame(xsp, id);
	  }

      }

      return -ENOTTY;
}


int ucr_irq(struct Instance*xsp)
{
      unsigned long mask = dev_get_bells(xsp);

	/* This bell happens when the target board responds to my
	   changing the root table. */
      if (mask & ROOT_TABLE_BELLMASK)
	    wake_up(&xsp->root_sync);

	/* This bell happens when the target board responds to my
	   sending a status signal. */
      if (mask & STATUS_BELLMASK)
	    ;

	/* This bell happens when it tells me that *it* has changed a
	   channel table. */
      if (mask & CHANGE_BELLMASK)
	    wake_up(&xsp->dispatch_sync);

      return mask != 0;
}
