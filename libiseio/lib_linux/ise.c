/*
 * Copyright (c) 2009 Picture Elements, Inc.
 *    Stephen Williams (steve@icarus.com)
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

# include  <libiseio.h>
# include  "priv.h"
# include  <assert.h>

/*
 * This is the implementation of libiseio that uses the ise device
 * driver under Linux.
 */

# include  <ucrif.h>
# include  <string.h>
# include  <unistd.h>
# include  <fcntl.h>
# include  <sys/mman.h>
# include  <sys/types.h>
# include  <errno.h>

static unsigned get_board_id(struct ise_handle*dev)
{
      return strtoul(dev->id_str+3,0,10);
}

static int probe_id_ise(struct ise_handle*dev)
{
      char*cp;
      if (dev->id_str[0] != 'i') return 0;
      if (dev->id_str[1] != 's') return 0;
      if (dev->id_str[2] != 'e') return 0;
      if (dev->id_str[3] ==  0 ) return 0;

      cp = dev->id_str+3;
      while (*cp) {
	    if (! isdigit(*cp)) return 0;
	    cp += 1;
      }

      return 1;
}

static ise_error_t connect_ise(struct ise_handle*dev)
{
      char pathx[16];

      sprintf(pathx, "/dev/isex%u", get_board_id(dev));

      if (__ise_logfile)
	    fprintf(__ise_logfile, "%s: Opening control device %s\n",
		    dev->id_str, pathx);

      dev->isex = open(pathx, O_RDWR, 0);

      if (dev->isex < 0)
	    return ISE_ERROR;

      return ISE_OK;
}

static ise_error_t restart_ise(struct ise_handle*dev)
{
      if (dev->isex >= 0)
	    ioctl(dev->isex, UCRX_RESTART_BOARD, 0);
      return ISE_OK;
}

static ise_error_t run_program_ise(struct ise_handle*dev)
{
      unsigned wait_count;
      int rc;

      wait_count = 100;
      do {
	      /* Run! Run! */
	    rc = ioctl(dev->isex, UCRX_RUN_PROGRAM, 0);

	    if (rc >= 0) break;

	    if (__ise_logfile) {
		  fprintf(__ise_logfile, "%s: run failed, errno=%d"
			  " (wait_count=%u)\n", dev->id_str, errno, wait_count);
		  fflush(__ise_logfile);
	    }

	    usleep(20000);
	    wait_count -= 1;
      } while (wait_count > 0);

      if (rc >= 0) {

	    if (__ise_logfile) {
		  fprintf(__ise_logfile, "%s: **** ise_restart complete\n",
			  dev->id_str);
		  fflush(__ise_logfile);
	    }

	    return ISE_OK;

      } else {

	    if (__ise_logfile) {
		  fprintf(__ise_logfile, "%s: **** ise_restart failed"
			  " errno=%d\n", dev->id_str, errno);
		  fflush(__ise_logfile);
	    }

	    return ISE_ERROR;
      }
}

static ise_error_t channel_open_ise(struct ise_handle*dev,
				  struct ise_channel*chn)
{
      int fd;
      int rc;

      char path[32];
      sprintf(path, "/dev/ise%u", get_board_id(dev));
      fd = open(path, O_RDWR, 0);

      if (fd < 0)
	    return ISE_ERROR;

      rc = ioctl(fd, UCR_CHANNEL, chn->cid);
      if (rc < 0) {
	    chn->fd = -1;
	    close(fd);
	    return ISE_CHANNEL_BUSY;
      }

      chn->fd = fd;
      return ISE_OK;
}

static ise_error_t channel_sync_ise(struct ise_handle*dev,
				    struct ise_channel*chn)
{
      ioctl(chn->fd, UCR_SYNC, 0);
      return ISE_OK;
}

static ise_error_t channel_close_ise(struct ise_handle*dev,
				     struct ise_channel*chn)
{
      close(chn->fd);
      chn->fd = -1;
      return ISE_OK;
}

static ise_error_t timeout_ise(struct ise_handle*dev, unsigned cid,
			       long read_timeout)
{
      int rc;
      struct ucrx_timeout_s ts;

      ts.id = cid;
      ts.read_timeout = read_timeout;
      rc = ioctl(dev->isex, UCRX_TIMEOUT, &ts);
      if (rc < 0)
	    return ISE_ERROR;

      return ISE_OK;
}

static ise_error_t make_frame_ise(struct ise_handle*dev, unsigned id)
{
      size_t siz = dev->frame[id].size;
      unsigned long arg = (id << 28) | (siz & 0x0fffffffUL);
      int rc;

      rc = ioctl(dev->clist->fd, UCR_MAKE_FRAME, arg);
      if (rc < 0) {
	    dev->frame[id].base = 0;
	    return ISE_ERROR;
      }

      dev->frame[id].size = rc;
      dev->frame[id].base = mmap(0, siz, PROT_READ|PROT_WRITE,
				 MAP_SHARED, dev->clist->fd, (id << 28));

      return ISE_OK;
}

static void delete_frame_ise(struct ise_handle*dev, unsigned id)
{
      unsigned long arg = (id << 28) | (dev->frame[id].size & 0x0fffffffUL);
      int rc;

      munmap(dev->frame[id].base, dev->frame[id].size);
      dev->frame[id].base = 0;

      assert(dev->clist);   
      rc = ioctl(dev->clist->fd, UCR_FREE_FRAME, arg);
      if (rc < 0)
	    fprintf(stderr, "UCR_FREE_FRAME error %d\n", errno);
}

static ise_error_t write_ise(struct ise_handle*dev,
			     struct ise_channel*chn,
			     const void*buf, size_t nbuf)
{
      write(chn->fd, buf, nbuf);
      return ISE_OK;
}

static ise_error_t writeln_ise(struct ise_handle*dev,
			       struct ise_channel*chn,
			       const char*text)
{
      int rc;
      char nl;

      rc = write(chn->fd, text, strlen(text));
      nl = '\n';
      rc = write(chn->fd, &nl, 1);

      if (__ise_logfile) {
	    fprintf(__ise_logfile, "%s.%u: writeln FLUSH\n",
		    dev->id_str, chn->cid);
	    fflush(__ise_logfile);
      }

      rc = ioctl(chn->fd, UCR_FLUSH, 0);

      return ISE_OK;
}

static ise_error_t readbuf_ise(struct ise_handle*dev,
			       struct ise_channel*chn)
{
      int rc;

      rc = read(chn->fd, chn->buf, sizeof chn->buf);

      if (__ise_logfile) {
	    fprintf(__ise_logfile, "%s.%u: read returned (%d)...\n",
		    dev->id_str, chn->cid, rc);
	    fflush(__ise_logfile);
      }

      if (rc <= 0) {
	    if (__ise_logfile) {
		  fprintf(__ise_logfile, "%s.%u: readln read error\n",
			  dev->id_str, chn->cid);
		  fflush(__ise_logfile);
	    }

	    return ISE_ERROR;
      }

      chn->ptr = 0;
      chn->fill = rc;
      return ISE_OK;
}


const struct ise_driver_functions __driver_ise = {
 probe_id: probe_id_ise,
 connect: connect_ise,
 restart: restart_ise,
 run_program: run_program_ise,

 channel_open: channel_open_ise,
 channel_close: channel_close_ise,
 channel_sync: channel_sync_ise,
 timeout: timeout_ise,

 make_frame:  make_frame_ise,
 delete_frame: delete_frame_ise,

 write: write_ise,
 writeln: writeln_ise,
 readbuf: readbuf_ise,
};
