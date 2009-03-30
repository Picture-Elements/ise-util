/*
 * Copyright (c) 2000-2002 Picture Elements, Inc.
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
#ident "$Id: libiseio.c,v 1.14 2007/02/15 20:12:49 steve Exp $"

# include  <libiseio.h>
# include  <ucrif.h>
# include  <stdlib.h>
# include  <unistd.h>
# include  <stdio.h>
# include  <string.h>
# include  <errno.h>
# include  <sys/mman.h>
# include  <sys/types.h>
# include  <fcntl.h>
# include  <assert.h>

#ifndef FIRM_ROOT
# define FIRM_ROOT "/usr/share/ise"
#endif

static FILE*lib_logfile = 0;

struct ise_channel {
      struct ise_channel*next;
      unsigned cid;
      int fd;
      char buf[1024];
      unsigned ptr, fill;
};

struct ise_handle {
      unsigned id;
      int isex;
      char*version;
      struct ise_channel*clist;

      struct {
	    void*base;
	    size_t size;
      } frame[16];
};

const char*ise_error_msg(ise_error_t code)
{
      switch (code) {
	  case ISE_OK:
	    return "no error";
	  case ISE_ERROR:
	    return "unspecified error";
	  case ISE_NO_SCOF:
	    return "unable to read SCOF firmware";
	  case ISE_NO_CHANNEL:
	    return "no such open channel";
	  case ISE_CHANNEL_BUSY:
	    return "channel is busy or locked";

	  default:
	    return "unknown ise error code";
      }
}

const char*ise_prom_version(struct ise_handle*dev)
{
      return dev->version;
}

ise_error_t ise_restart(struct ise_handle*dev, const char*firm)
{
      unsigned wait_count;
      char path[1024];
      int ch0, fd, rc;

      if (lib_logfile)
	    fprintf(lib_logfile, "ise%u: **** ise_restart(...,%s)\n",
		    dev->id, firm);

      ioctl(dev->isex, UCRX_RESTART_BOARD, 0);

	/* Open channel 0 to the firmware. This will be where I shove
	   the firmware. */

      if (lib_logfile)
	    fprintf(lib_logfile, "ise%u: open channel 0\n", dev->id);

      sprintf(path, "/dev/ise%u", dev->id);
      ch0 = open(path, O_RDWR, 0);
      rc = ioctl(ch0, UCR_CHANNEL, 0);

	/* Try to find the scof file. Look in the current working
	   directory and the compiled in library directory. */

      sprintf(path, "./%s.scof", firm);
      if (lib_logfile)
	    fprintf(lib_logfile, "ise%u: try firmware %s\n",
		    dev->id, path);

      fd = open(path, O_RDONLY, 0);
      if (fd < 0) {
	    sprintf(path, FIRM_ROOT "/%s.scof", firm);

	    if (lib_logfile)
		  fprintf(lib_logfile, "ise%u: try firmware %s\n",
			  dev->id, path);

	    fd = open(path, O_RDONLY, 0);
      }

      if (fd < 0) {
	    if (lib_logfile)
		  fprintf(lib_logfile, "ise%u: **** No firmware, "
			  "giving up.\n", dev->id, path);

	    close(ch0);
	    return ISE_NO_SCOF;
      }

      if (lib_logfile)
	    fprintf(lib_logfile, "ise%u: transmitting firmware\n", dev->id);

	/* In a loop, read bytes of the SCOF file and write them into
	   channel 0. This causes the flash to load the firmware into
	   DRAM at the correct places. */
      rc = read(fd, path, sizeof path);
      while (rc > 0) {
	    write(ch0, path, rc);
	    rc = read(fd, path, sizeof path);
      }

      close(fd);

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u: sync channel 0...\n", dev->id);
	    fflush(lib_logfile);
      }

	/* Flush the data in the channel, and close the channel. We
	   are done writing the program into the board. The SYNC is
	   needed to make sure all the bytes are on the target board
	   before the channel buffers are removed by the close. */
      rc = ioctl(ch0, UCR_SYNC, 0);
      close(ch0);

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u: running program\n", dev->id);
	    fflush(lib_logfile);
      }

      wait_count = 100;
      do {
	      /* Run! Run! */
	    rc = ioctl(dev->isex, UCRX_RUN_PROGRAM, 0);

	    if (rc >= 0) break;

	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u: run failed, errno=%d"
			  " (wait_count=%u)\n", dev->id, errno, wait_count);
		  fflush(lib_logfile);
	    }

	    usleep(20000);
	    wait_count -= 1;
      } while (wait_count > 0);

      if (rc >= 0) {

	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u: **** ise_restart complete\n",
			  dev->id);
		  fflush(lib_logfile);
	    }

	    return ISE_OK;

      } else {

	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u: **** ise_restart failed"
			  " errno=%d\n", dev->id, errno);
		  fflush(lib_logfile);
	    }

	    return ISE_ERROR;
      }
}

ise_error_t ise_channel(struct ise_handle*dev, unsigned cid)
{
      struct ise_channel*chn;
      int fd;
      int rc;

      { char path[32];
        sprintf(path, "/dev/ise%u", dev->id);
	fd = open(path, O_RDWR, 0);
      }

      if (fd < 0) {
	    return ISE_ERROR;
      }

      rc = ioctl(fd, UCR_CHANNEL, cid);
      if (rc < 0) {
	    close(fd);
	    return ISE_CHANNEL_BUSY;
      }

      chn = calloc(1, sizeof (struct ise_channel));
      chn->cid = cid;
      chn->fd  = fd;
      chn->ptr = 0;
      chn->fill = 0;
      chn->next = dev->clist;
      dev->clist = chn;

      return ISE_OK;
}

void* ise_make_frame(struct ise_handle*dev, unsigned id, size_t*siz)
{
      long rc;
      unsigned long arg = (id << 28) | (*siz & 0x0fffffffUL);

      if (dev->frame[id].base) {
	    *siz = dev->frame[id].size;
	    return dev->frame[id].base;
      }

      if (dev->clist == 0)
	    return 0;

      rc = ioctl(dev->clist->fd, UCR_MAKE_FRAME, arg);
      if (rc < 0)
	    return 0;

      dev->frame[id].size = rc;
      dev->frame[id].base = mmap(0, *siz, PROT_READ|PROT_WRITE,
				 MAP_SHARED, dev->clist->fd, (id << 28));

      return dev->frame[id].base;
}

void ise_delete_frame(struct ise_handle*dev, unsigned id)
{
      int rc;
      unsigned long arg = (id << 28) | (dev->frame[id].size & 0x0fffffffUL);

      assert(dev);

      if (dev->frame[id].base == 0)
	    return;

      munmap(dev->frame[id].base, dev->frame[id].size);
      dev->frame[id].base = 0;

      assert(dev->clist);   
      rc = ioctl(dev->clist->fd, UCR_FREE_FRAME, arg);
      if (rc < 0)
	    fprintf(stderr, "UCR_FREE_FRAME error %d\n", errno);
}

ise_error_t ise_writeln(struct ise_handle*dev, unsigned cid,
			const char*text)
{
      struct ise_channel*chn = dev->clist;
      int rc;

      while (chn && (chn->cid != cid))
	    chn = chn->next;

      if (chn == 0)
	    return ISE_NO_CHANNEL;

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u.%u: writeln(%s)\n",
		    dev->id, chn->cid, text);
	    fflush(lib_logfile);
      }

      rc = write(chn->fd, text, strlen(text));
      { char nl = '\n';
        rc = write(chn->fd, &nl, 1);
      }

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u.%u: writeln FLUSH\n",
		    dev->id, chn->cid);
	    fflush(lib_logfile);
      }

      rc = ioctl(chn->fd, UCR_FLUSH, 0);

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u.%u: writeln done\n",
		    dev->id, chn->cid);
	    fflush(lib_logfile);
      }

      return ISE_OK;
}

ise_error_t ise_readln(struct ise_handle*dev, unsigned cid,
		       char*buf, size_t nbuf)
{
      struct ise_channel*chn = dev->clist;
      int rc;
      char*bp;

      while (chn && (chn->cid != cid))
	    chn = chn->next;

      if (chn == 0)
	    return ISE_NO_CHANNEL;

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u.%u: readln...\n",
		    dev->id, chn->cid, buf);
	    fflush(lib_logfile);
      }

      bp = buf;
      do {
	    while (chn->fill > 0) {
		  *bp = chn->buf[chn->ptr];
		  chn->ptr += 1;
		  chn->fill -= 1;
		  if (*bp == '\n') {
			*bp = 0;

			if (lib_logfile) {
			      fprintf(lib_logfile, "ise%u.%u: readln -->%s\n",
				      dev->id, chn->cid, buf);
			      fflush(lib_logfile);
			}

			return ISE_OK;
		  }
		  bp += 1;
	    }

	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u.%u: read more data...\n",
			  dev->id, chn->cid);
		  fflush(lib_logfile);
	    }

	    rc = read(chn->fd, chn->buf, sizeof chn->buf);

	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u.%u: read returned (%d)...\n",
			  dev->id, chn->cid, rc);
		  fflush(lib_logfile);
	    }

	    if (rc <= 0) {
		  *bp = 0;
		  if (lib_logfile) {
			fprintf(lib_logfile, "ise%u.%u: readln read error\n",
				dev->id, chn->cid);
			fflush(lib_logfile);
		  }

		  return ISE_ERROR;
	    }

	    chn->ptr = 0;
	    chn->fill = rc;

      } while (bp < (buf+nbuf));

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u.%u: readln buffer overrun\n",
		    dev->id, chn->cid, buf);
	    fflush(lib_logfile);
      }

      return ISE_ERROR;
}

ise_error_t ise_timeout(struct ise_handle*dev, unsigned cid,
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

struct ise_handle*ise_bind(const char*name)
{
      struct ise_handle*dev;
      unsigned id = 0;
      char*logpath;

      if ((lib_logfile == 0) && ((logpath = getenv("LIBISEIO_LOG")))) {
	    char*cp = strchr(logpath, '=');
	    if (cp)
		  cp += 1;
	    else
		  cp = logpath;

	    if (strcmp(logpath,"-") == 0) {
		  lib_logfile = stdout;

	    } else if (strcmp(logpath,"--") == 0) {
		  lib_logfile = stderr;

	    } else {
		  lib_logfile = fopen(logpath, "a");
	    }
      }

      if (lib_logfile)
	    fprintf(lib_logfile, "ise: **** ise_bind(%s)\n", name);

      if (name[0] != 'i') return 0;
      if (name[1] != 's') return 0;
      if (name[2] != 'e') return 0;
      if (name[3] ==  0 ) return 0;

      { const char*cp = name+3;
        while (*cp) {
	      if (! isdigit(*cp)) return 0;
	      id *= 10;
	      id += *cp - '0';
	      cp += 1;
	}
      }

      dev = calloc(1, sizeof(struct ise_handle));
      dev->id = id;
      dev->version = 0;
      dev->isex = -1;
      dev->clist = 0;

      return dev;
}

struct ise_handle*ise_open(const char*name)
{
      struct ise_handle*dev;
      char*buf = 0;
      size_t nbuf = 0;
      int mon;
      char*logpath;

      dev = ise_bind(name);

      if (lib_logfile)
	    fprintf(lib_logfile, "ise: **** ise_open(%s)\n", name);

      { char pathx[16];
        sprintf(pathx, "/dev/isex%u", dev->id);

	if (lib_logfile)
	      fprintf(lib_logfile, "ise%u: Opening control device %s\n",
		      dev->id, pathx);

	dev->isex = open(pathx, O_RDWR, 0);
      }

      if (dev->isex < 0) {
	    free(dev);

	    if (lib_logfile)
		  fprintf(lib_logfile, "ise%u: Control file failed.\n", dev->id);

	    return 0;
      }

      if (lib_logfile)
	    fprintf(lib_logfile, "ise%u: Restart device\n", dev->id);

      ioctl(dev->isex, UCRX_RESTART_BOARD, 0);

      { char path[16];
        sprintf(path, "/dev/ise%u", dev->id);

	if (lib_logfile)
	      fprintf(lib_logfile, "ise%u: Open device %s\n", dev->id, path);

	mon = open(path, O_RDWR, 0);

	if (lib_logfile)
	      fprintf(lib_logfile, "ise%u: switch to channel 254\n", dev->id);

	ioctl(mon, UCR_CHANNEL, 254);
      }

      if (lib_logfile)
	    fprintf(lib_logfile, "ise%u: reading ident data\n", dev->id);

      write(mon, "\n", 1);
      ioctl(mon, UCR_FLUSH, 0);

      buf = malloc(4096);
      nbuf = 4096;

      buf[0] = 0;
      while (buf[0] != '>') {
	    read(mon, buf, 1);
      }

	/* Send the [i]dent command to the prom monitor. */
      write(mon, "i\n", 2);
      ioctl(mon, UCR_FLUSH, 0);

	/* Read back the ident string. Read the whole thing, up to the
	   final prompt (>) character. Make sure the allocate more
	   buffer as needed. */
      { char*cp = buf;
        for (;;) {
	      if ((cp-buf) == nbuf) {
		    long dif = cp - buf;
		    buf = realloc(buf, nbuf+1024);
		    cp = buf + dif;
		    nbuf += 1024;
	      }

	      read(mon, cp, 1);
	      if (cp[0] == '>') {
		    cp[0] = 0;
		    break;
	      }

	      cp += 1;
	}

	  /* All done reading the version information. Trim the
	     allocated space back down to the exact size, and stash the
	     data away as the version string. */
	buf = realloc(buf, (cp-buf) + 1);
	dev->version = buf;
      }

      if (lib_logfile)
	    fprintf(lib_logfile, "ise%u: Restart device\n", dev->id);

	/* All done with this setup. Close the monitor channel and
	   reset the ISE board. */
      close(mon);
      ioctl(dev->isex, UCRX_RESTART_BOARD, 0);

      if (lib_logfile)
	    fprintf(lib_logfile, "ise%u: ise_open(%s) complete.\n",
		    dev->id, name);

      return dev;
}

void ise_close(struct ise_handle*dev)
{
      unsigned idx;

      if (lib_logfile)
	    fprintf(lib_logfile, "ise%u: **** ise_close\n", dev->id);

      for (idx = 0 ;  idx < 16 ;  idx += 1) {
	    if (dev->frame[idx].base == 0)
		  continue;

	    if (lib_logfile)
		  fprintf(lib_logfile, "ise%u: delete frame %u\n",
			  dev->id, idx);

	    ise_delete_frame(dev, idx);
      }

      while (dev->clist) {
	    struct ise_channel*chn = dev->clist;
	    dev->clist = chn->next;

	    if (lib_logfile)
		  fprintf(lib_logfile, "ise%u: Close channel %u\n",
			  dev->id, chn->cid);

	    close(chn->fd);
	    free(chn);
      }

      if (dev->version) {
	    if (lib_logfile)
		  fprintf(lib_logfile, "ise%u: Reset board.\n", dev->id);

	    if (dev->isex >= 0) {
		  ioctl(dev->isex, UCRX_RESTART_BOARD, 0);
		  close(dev->isex);
	    }

	    free(dev->version);

      } else {
	      /* If the board was opened by ise_bind, then skip the
		 reset. */
	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u: Opened by ise_bind, "
			  "so skipping reset.\n", dev->id);
		  fflush(lib_logfile);
	    }
      }

      if (lib_logfile)
	    fprintf(lib_logfile, "ise%u: **** ise_close complete\n", dev->id);

      free(dev);
}

/*
 * $Log: libiseio.c,v $
 * Revision 1.14  2007/02/15 20:12:49  steve
 *  Skip restart if using ise_bind.
 *
 * Revision 1.13  2005/08/09 22:48:01  steve
 *  Fix access of the wrong device in ise_restart.
 *
 * Revision 1.12  2005/08/09 00:10:25  steve
 *  Add the ise_bind function.
 *
 * Revision 1.11  2004/04/02 03:30:46  steve
 *  Do not use UCRX_RESET, it is not needed.
 *
 * Revision 1.10  2003/04/25 22:38:40  steve
 *  Add more logs.
 *
 * Revision 1.9  2002/11/15 19:30:12  steve
 *  Account for buffer move after realloc.
 *
 * Revision 1.8  2002/06/13 00:44:34  steve
 *  Delete frames on device close.
 *
 * Revision 1.7  2002/04/11 22:00:40  steve
 *  Retry in run to close race opening.
 *
 * Revision 1.6  2002/03/26 22:07:56  steve
 *  Support a library log dump file.
 *
 * Revision 1.5  2001/07/16 19:59:36  steve
 *  Make timeout features of driver available to API.
 *
 * Revision 1.4  2001/03/27 22:21:51  steve
 *  Add the ise_make_frame function.
 *
 * Revision 1.3  2000/07/24 20:38:47  steve
 *  Fix scanning for scof files.
 *
 * Revision 1.2  2000/06/29 22:46:54  steve
 *  Close dangling fd.
 *
 * Revision 1.1  2000/06/27 22:51:22  steve
 *  Add libiseio for Linux.
 *
 */

