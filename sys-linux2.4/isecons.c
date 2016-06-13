/*
 * Copyright (c) 2002-2004 Picture Elements, Inc.
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
# include  <linux/vmalloc.h>
# include  <linux/kernel.h>
# include  <linux/proc_fs.h>

# define LOG_BUF_SIZE (16*1024)

static struct proc_dir_entry*proc_isecons = 0;

static struct isecons_s {
      char*log_buf;
      unsigned ptr;
      unsigned fill;
} cons = { 0, 0, 0 };

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static ssize_t consread(struct file *file, char *bytes, size_t count, loff_t*off);

static ssize_t conswrite(struct file*file, const char*bytes,
			 size_t count, loff_t*off);

static struct file_operations isecons_fop = {
 read: consread,
 write: conswrite
};

void isecons_init(void)
{
      cons.log_buf = vmalloc(LOG_BUF_SIZE);
      proc_isecons = proc_create_data("driver/isecons",
				      S_IFREG|S_IRUGO|S_IWUGO, 0,
				      &isecons_fop, 0);

      cons.ptr = 0;
      cons.fill = 0;
}

#else

static int isecons_read(char*page, char**start, off_t offset, int count,
			int*eof, void*data);
static int isecons_write(struct file*file, const char __user*buffer,
			 unsigned long count, void*data);
void isecons_init(void)
{
      cons.log_buf = vmalloc(LOG_BUF_SIZE);
      proc_isecons = create_proc_entry("driver/isecons", S_IFREG|S_IRUGO|S_IWUGO, 0);
      if (proc_isecons) {
	    proc_isecons->read_proc = isecons_read;
	    proc_isecons->write_proc = isecons_write;
	    proc_isecons->data = "isecons";
      }

      cons.ptr = 0;
      cons.fill = 0;
}

#endif


void isecons_release(void)
{
      remove_proc_entry("driver/isecons", 0);
      vfree(cons.log_buf);
}

void isecons_log(const char*fmt, ...)
{
      va_list args;
      static char data_buf[1024];
      char*data;
      int ndata;

      if (cons.log_buf == 0) {
	    return;
      }

      va_start(args, fmt);
      ndata = vsprintf(data_buf, fmt, args);
      data = data_buf;
      va_end(args);

      if (ndata == 0) return;

	/* If this message alone will overflow the buffer, take only
	   the tail part that will fit. */

      if (ndata > LOG_BUF_SIZE) {
	    data += (ndata - LOG_BUF_SIZE);
	    ndata = LOG_BUF_SIZE;
      }

	/* Remove enough from the front of the buffer that this
	   message will fit. */

      if ((cons.fill+ndata) > LOG_BUF_SIZE) {
	    unsigned remove = (cons.fill+ndata) - LOG_BUF_SIZE;
	    cons.ptr = (cons.ptr + remove) % LOG_BUF_SIZE;
	    cons.fill -= remove;

	    printk("isecons: log buffer overflow\n");
      }

      while (ndata > 0) {
	    unsigned cur = (cons.ptr + cons.fill) % LOG_BUF_SIZE;
	    unsigned tcount = ndata;

	      /* Watch for wrapping... */
	    if ((cur + tcount) > LOG_BUF_SIZE)
		  tcount = LOG_BUF_SIZE - cur;

	      /* Do the somewhat constrained write into the buffer. */
	    memcpy(cons.log_buf+cur, data, tcount);
	    cons.fill += tcount;
	    data += tcount;
	    ndata -= tcount;
      }
}

static int isecons_read(char*page, char**start, off_t offset, int count,
			int*eof, void*data)
{
      int trans;

      if (cons.fill == 0) {
	    *eof = 1;
	    return 0;
      }

      trans = count;
      if (cons.fill < trans) {
	    trans = cons.fill;
      }

      if ((cons.ptr + trans) > LOG_BUF_SIZE) {
	    trans = LOG_BUF_SIZE - cons.ptr;
      }

      memcpy(page, cons.log_buf + cons.ptr, trans);
      cons.ptr += trans;
      cons.fill -= trans;

      *start = page;

      if (cons.ptr == LOG_BUF_SIZE)
	    cons.ptr = 0;
      if (cons.fill == 0)
	    *eof = 1;

      return trans;
}

static int isecons_write(struct file*file, const char __user*buffer,
			 unsigned long count, void*data)
{
      static char line_buffer[1024];
      const char*beg = buffer;
      const char*end;

	/* Look in the string for the beginning of the next command. */
      while (beg[0] != '<') {
	    beg += 1;
	    if ((beg-buffer) >= count)
		  return count;
      }

	/* Look for the command terminator, the EOL. */
      end = beg + 1;
      while (end[0] != '\n') {
	    end += 1;
	    if ((end-buffer) >= count)
		  break;
      }

      if (beg > buffer) {
	    printk(KERN_INFO "isecons: skip %zd bytes of leading data.\n",
		   beg - buffer);
      }

      if (end < (buffer + count - 1)) {
	    printk(KERN_INFO "isecons: skip %lu bytes of trailing data.\n",
		   count - (end-buffer));
      }

	/* Clip the line to the size of the line buffer, ... */
      if ((end-beg) >= sizeof line_buffer)
	    end = beg + sizeof line_buffer - 1;

	/* ... and copy it in string form. */
      memcpy(line_buffer, beg, end-beg);
      line_buffer[end-beg] = 0;

      if (strncmp(line_buffer, "<log>",5) == 0) {
	    isecons_log("%s\n", line_buffer+5);
	    return count;
      }

      if (strncmp(line_buffer, "<bell0>", 7) == 0) {
	    unsigned long mask = simple_strtoul(line_buffer+7, 0, 0);
	    struct Instance*xsp = ucr_find_board_instance(0);
	    isecons_log("<bell0>0x%lx\n", mask);
	    if (xsp) dev_set_bells(xsp, mask);
	    return count;
      }

      if (strncmp(line_buffer, "<dump-root>", 11) == 0) {
	    int idx;
	    struct Instance*xsp = ucr_find_board_instance(0);
	    if (xsp->root == 0) {
		  isecons_log("NO ROOT TABLE\n");
		  return count;
	    }

	    isecons_log("root magic: 0x%08x:%08x\n", xsp->root->magic, xsp->root->self);
	    for (idx = 0 ; idx < 16 ; idx += 1) {
		  isecons_log("frame[%2d]: 0x%08x:%08x\n", idx,
			      xsp->root->frame_table[idx].ptr,
			      xsp->root->frame_table[idx].magic);
	    }
	    for (idx = 0 ; idx < ROOT_TABLE_CHANNELS ; idx += 1) {
		  isecons_log("chan[%3d]: 0x%08x:%08x\n", idx,
			      xsp->root->chan[idx].ptr,
			      xsp->root->chan[idx].magic);
	    }
	    return count;
      }
      return count;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static ssize_t consread(struct file *file, char *bytes, size_t count, loff_t*off)
{
      static char buf[PAGE_SIZE];
      int eof = 0;
      ssize_t total = 0;
      while (count > 0 && eof==0) {
	    char*startp = 0;
	    ssize_t rc;
	    size_t trans = count;
	    if (trans > sizeof buf)
		  trans = sizeof buf;

	    rc = isecons_read(buf, &startp, 0, trans, &eof, 0);
	    if (rc > 0) {
		  if (copy_to_user(bytes, startp, rc) != 0)
			return -EFAULT;
		  count -= rc;
		  bytes += rc;
		  total += rc;
	    }
      }

      return total;
}

static ssize_t conswrite(struct file*file, const char*bytes,
			 size_t count, loff_t*off)
{
      static char buf[PAGE_SIZE];
      ssize_t total = 0;

      while (count > 0) {
	    int rc;
	    size_t trans = count;
	    if (trans > sizeof buf)
		  trans = sizeof buf;

	    if (copy_from_user(buf, bytes, trans) != 0)
		  return -EFAULT;

	    rc = isecons_write(file, buf, trans, 0);
	    if (rc > 0) {
		  count -= rc;
		  total += rc;
		  bytes += rc;
	    }
      }

      return total;
}
#endif
