/*
 * Copyright (c) 2000-2009 Picture Elements, Inc.
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

# include  <libiseio.h>
# include  "priv.h"

# include  <stdlib.h>
# include  <unistd.h>
# include  <stdio.h>
# include  <string.h>
# include  <errno.h>
# include  <fcntl.h>

# include  <assert.h>

#ifndef FIRM_ROOT
# define FIRM_ROOT "/usr/share/ise"
#endif

FILE*__ise_logfile = 0;

struct ise_channel* __ise_find_channel(struct ise_handle*dev, unsigned cid)
{
      struct ise_channel*chn = dev->clist;

      while (chn && chn->cid != cid)
	    chn = chn->next;

      return chn;
}

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
      char path[1024];
      int fd, rc;
      ise_error_t rc_ise;

      struct ise_channel ch0;

      if (__ise_logfile)
	    fprintf(__ise_logfile, "%s: **** ise_restart(...,%s)\n",
		    dev->id_str, firm);

      dev->fun->restart(dev);
      
	/* Open channel 0 to the firmware. This will be where I shove
	   the firmware. */

      if (__ise_logfile)
	    fprintf(__ise_logfile, "%s: open channel 0\n", dev->id_str);

      ch0.next = 0;
      ch0.cid  = 0;
      ch0.fd   = -1;
      ch0.ptr  = 0;
      ch0.fill = 0;
      rc_ise = dev->fun->channel_open(dev, &ch0);
      if (rc_ise != ISE_OK)
	    return rc_ise;

	/* Try to find the scof file. Look in the current working
	   directory and the compiled in library directory. */

      sprintf(path, "./%s.scof", firm);
      if (__ise_logfile)
	    fprintf(__ise_logfile, "%s: try firmware %s\n",
		    dev->id_str, path);

      fd = open(path, O_RDONLY, 0);
      if (fd < 0) {
	    sprintf(path, FIRM_ROOT "/%s.scof", firm);

	    if (__ise_logfile)
		  fprintf(__ise_logfile, "%s: try firmware %s\n",
			  dev->id_str, path);

	    fd = open(path, O_RDONLY, 0);
      }

      if (fd < 0) {
	    if (__ise_logfile)
		  fprintf(__ise_logfile, "%s: **** No firmware, "
			  "giving up.\n", dev->id_str, path);

	    dev->fun->channel_close(dev, &ch0);
	    return ISE_NO_SCOF;
      }

      if (__ise_logfile)
	    fprintf(__ise_logfile, "%s: transmitting firmware\n", dev->id_str);

	/* In a loop, read bytes of the SCOF file and write them into
	   channel 0. This causes the flash to load the firmware into
	   DRAM at the correct places. */
      rc = read(fd, path, sizeof path);
      while (rc > 0) {
	    dev->fun->write(dev, &ch0, path, rc);
	    rc = read(fd, path, sizeof path);
      }

      close(fd);

      if (__ise_logfile) {
	    fprintf(__ise_logfile, "%s: sync channel 0...\n", dev->id_str);
	    fflush(__ise_logfile);
      }

	/* Flush the data in the channel, and close the channel. We
	   are done writing the program into the board. The SYNC is
	   needed to make sure all the bytes are on the target board
	   before the channel buffers are removed by the close. */
      dev->fun->channel_sync(dev, &ch0);
      dev->fun->channel_close(dev, &ch0);

      if (__ise_logfile) {
	    fprintf(__ise_logfile, "%s: running program\n", dev->id_str);
	    fflush(__ise_logfile);
      }

      return dev->fun->run_program(dev);
}

ise_error_t ise_channel(struct ise_handle*dev, unsigned cid)
{
      struct ise_channel*chn;
      ise_error_t rc;

      chn = calloc(1, sizeof (struct ise_channel));
      chn->cid = cid;
      chn->fd  = -1;
      chn->ptr = 0;
      chn->fill = 0;

      rc = dev->fun->channel_open(dev, chn);
      if (rc != ISE_OK) {
	    free(chn);
	    return rc;
      }

      chn->next = dev->clist;
      dev->clist = chn;

      return ISE_OK;
}

void* ise_make_frame(struct ise_handle*dev, unsigned id, size_t*siz)
{
      ise_error_t rc;

      if (dev->frame[id].base) {
	    *siz = dev->frame[id].size;
	    return dev->frame[id].base;
      }

      dev->frame[id].size = *siz;
      rc = dev->fun->make_frame(dev, id);
      if (rc != ISE_OK)
	    return 0;

      *siz = dev->frame[id].size;
      return dev->frame[id].base;
}

void ise_delete_frame(struct ise_handle*dev, unsigned id)
{
      assert(dev);

      if (dev->frame[id].base == 0)
	    return;

      dev->fun->delete_frame(dev, id);
}

ise_error_t ise_writeln(struct ise_handle*dev, unsigned cid,
			const char*text)
{
      struct ise_channel*chn = __ise_find_channel(dev, cid);
      int rc;

      if (chn == 0)
	    return ISE_NO_CHANNEL;

      if (__ise_logfile) {
	    fprintf(__ise_logfile, "%s.%u: writeln(%s)\n",
		    dev->id_str, chn->cid, text);
	    fflush(__ise_logfile);
      }

      return dev->fun->writeln(dev, chn, text);
}

ise_error_t ise_readln(struct ise_handle*dev, unsigned cid,
		       char*buf, size_t nbuf)
{
      struct ise_channel*chn = __ise_find_channel(dev, cid);
      ise_error_t rc;
      char*bp;

      if (chn == 0)
	    return ISE_NO_CHANNEL;

      if (__ise_logfile) {
	    fprintf(__ise_logfile, "%s.%u: readln...\n",
		    dev->id_str, chn->cid, buf);
	    fflush(__ise_logfile);
      }

      bp = buf;
      do {
	    while (chn->fill > 0) {
		  *bp = chn->buf[chn->ptr];
		  chn->ptr += 1;
		  chn->fill -= 1;
		  if (*bp == '\n') {
			*bp = 0;

			if (__ise_logfile) {
			      fprintf(__ise_logfile, "%s.%u: readln -->%s\n",
				      dev->id_str, chn->cid, buf);
			      fflush(__ise_logfile);
			}

			return ISE_OK;
		  }
		  bp += 1;
	    }

	    if (__ise_logfile) {
		  fprintf(__ise_logfile, "%s.%u: read more data...\n",
			  dev->id_str, chn->cid);
		  fflush(__ise_logfile);
	    }

	    rc = dev->fun->readbuf(dev, chn);

	    if (rc != ISE_OK) {
		  *bp = 0;
		  if (__ise_logfile) {
			fprintf(__ise_logfile, "%s.%u: readln read error\n",
				dev->id_str, chn->cid);
			fflush(__ise_logfile);
		  }

		  return rc;
	    }


      } while (bp < (buf+nbuf));

      if (__ise_logfile) {
	    fprintf(__ise_logfile, "%s.%u: readln buffer overrun\n",
		    dev->id_str, chn->cid, buf);
	    fflush(__ise_logfile);
      }

      return ISE_ERROR;
}

ise_error_t ise_timeout(struct ise_handle*dev, unsigned cid,
			long read_timeout)
{
      return dev->fun->timeout(dev, cid, read_timeout);
}

struct ise_handle*ise_bind(const char*name)
{
      struct ise_handle*dev;
      unsigned id = 0;
      char*logpath;

      if ((__ise_logfile == 0) && ((logpath = getenv("LIBISEIO_LOG")))) {
	    char*cp = strchr(logpath, '=');
	    if (cp)
		  cp += 1;
	    else
		  cp = logpath;

	    if (strcmp(logpath,"-") == 0) {
		  __ise_logfile = stdout;

	    } else if (strcmp(logpath,"--") == 0) {
		  __ise_logfile = stderr;

	    } else {
		  __ise_logfile = fopen(logpath, "a");
	    }
      }

      if (__ise_logfile)
	    fprintf(__ise_logfile, "ise: **** ise_bind(%s)\n", name);

      dev = calloc(1, sizeof(struct ise_handle));
      dev->id_str = strdup(name);
      dev->version = 0;
      dev->isex = -1;
      dev->clist = 0;

      if (__driver_ise.probe_id(dev) != 0)
	    dev->fun = &__driver_ise;
      else if (__driver_plug.probe_id(dev) != 0)
	    dev->fun = &__driver_plug;
      else {
	    free(dev->id_str);
	    free(dev);
	    return 0;
      }


      return dev;
}

struct ise_handle*ise_open(const char*name)
{
      struct ise_handle*dev;
      char*logpath;
      ise_error_t rc;
      struct ise_channel mon;

      dev = ise_bind(name);

      if (__ise_logfile)
	    fprintf(__ise_logfile, "ise: **** ise_open(%s)\n", name);

      rc = dev->fun->connect(dev);
      if (rc != ISE_OK) {

	    if (__ise_logfile)
		  fprintf(__ise_logfile, "%s: Control file failed.\n", dev->id_str);

	    free(dev->id_str);
	    free(dev);
	    return 0;
      }

      if (__ise_logfile)
	    fprintf(__ise_logfile, "%s: Restart device\n", dev->id_str);

      dev->fun->restart(dev);

      if (__ise_logfile)
	    fprintf(__ise_logfile, "%s: Open monitor port\n", dev->id_str);

      mon.fd = -1;
      mon.cid = 254;
      mon.ptr = 0;
      mon.fill = 0;
      rc = dev->fun->channel_open(dev, &mon);

      if (__ise_logfile)
	    fprintf(__ise_logfile, "%s: reading ident data\n", dev->id_str);

      dev->fun->writeln(dev, &mon, "");

      dev->fun->readbuf(dev, &mon);
      for (;;) {
	    char ch = mon.buf[mon.ptr];
	    mon.ptr += 1;
	    mon.fill -= 1;
	    if (ch == '>')
		  break;

	    if (mon.fill == 0)
		  dev->fun->readbuf(dev, &mon);
      }

	/* Send the [i]dent command to the prom monitor. */
      dev->fun->writeln(dev, &mon, "i");

	/* Read back the ident string. Read the whole thing, up to the
	   final prompt (>) character. Make sure the allocate more
	   buffer as needed. */
      dev->version = malloc(256);
      { char*cp = dev->version;
	size_t nbuf = 256;
        for (;;) {
	      if ((cp-dev->version) == nbuf) {
		    long dif = cp - dev->version;
		    dev->version = realloc(dev->version, nbuf+256);
		    cp = dev->version + dif;
		    nbuf += 256;
	      }

	      if (mon.fill == 0)
		    dev->fun->readbuf(dev, &mon);
	      if (mon.fill == 0) {
		    cp[0] = 0;
		    break;
	      }

	      cp[0] = mon.buf[mon.ptr];
	      mon.ptr += 1;
	      mon.fill -= 1;
	      if (cp[0] == '>') {
		    cp[0] = 0;
		    break;
	      }

	      cp += 1;
	}

	  /* All done reading the version information. Trim the
	     allocated space back down to the exact size, and stash the
	     data away as the version string. */
	dev->version = realloc(dev->version, (cp- dev->version) + 1);
      }

	/* All done with this setup. Close the monitor channel and
	   reset the ISE board. */
      dev->fun->channel_close(dev, &mon);

      if (__ise_logfile)
	    fprintf(__ise_logfile, "%s: Restart device\n", dev->id_str);

      dev->fun->restart(dev);

      if (__ise_logfile)
	    fprintf(__ise_logfile, "%s: ise_open(%s) complete.\n",
		    dev->id_str, name);

      return dev;
}

void ise_close(struct ise_handle*dev)
{
      unsigned idx;

      if (__ise_logfile)
	    fprintf(__ise_logfile, "%s: **** ise_close\n", dev->id_str);

      for (idx = 0 ;  idx < 16 ;  idx += 1) {
	    if (dev->frame[idx].base == 0)
		  continue;

	    if (__ise_logfile)
		  fprintf(__ise_logfile, "%s: delete frame %u\n",
			  dev->id_str, idx);

	    ise_delete_frame(dev, idx);
      }

      while (dev->clist) {
	    struct ise_channel*chn = dev->clist;
	    dev->clist = chn->next;

	    if (__ise_logfile)
		  fprintf(__ise_logfile, "%s: Close channel %u\n",
			  dev->id_str, chn->cid);

	    dev->fun->channel_close(dev, chn);
	    free(chn);
      }

      if (dev->version) {
	    if (__ise_logfile)
		  fprintf(__ise_logfile, "%s: Reset board.\n", dev->id_str);

	    dev->fun->restart(dev);

	    free(dev->version);

      } else {
	      /* If the board was opened by ise_bind, then skip the
		 reset. */
	    if (__ise_logfile) {
		  fprintf(__ise_logfile, "%s: Opened by ise_bind, "
			  "so skipping reset.\n", dev->id_str);
		  fflush(__ise_logfile);
	    }
      }

      if (__ise_logfile)
	    fprintf(__ise_logfile, "%s: **** ise_close complete\n", dev->id_str);

      free(dev->id_str);
      free(dev);
}
