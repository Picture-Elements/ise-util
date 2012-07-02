/*
 * Copyright (c) 2004 Picture Elements, Inc.
 *    Stephen Williams (steve@icarus.com)
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

# include  "libiseio_plug.h"
# include  "priv.h"
# include  <time.h>
# include  <string.h>
# include  <errno.h>
# include  <assert.h>

struct channel_data __libiseio_channels[256];

int ise_plug_readln(int chn, char*buf, size_t nbuf, long udelay)
{
      struct channel_data*chp = __libiseio_channels + chn;
      char*nl;
      int rc = 0;

      if (__libiseio_plug_log)
	    fprintf(__libiseio_plug_log, "ise_plug_readln: "
		    "readln nbuf=%zu, udelay=%ld\n", nbuf, udelay);

	/* Calculate the timeout time as an absolute time. */
      struct timespec timeout;
      if (udelay > 0) {
	    clock_gettime(CLOCK_REALTIME, &timeout);
	    timeout.tv_sec += udelay/1000000;
	    timeout.tv_nsec += 1000 * (udelay%1000000);
	    if (timeout.tv_nsec >= 1000000000L) {
		  timeout.tv_nsec -= 1000000000L;
		  timeout.tv_sec += 1;
	    }
      }

      pthread_mutex_lock(&chp->sync);

	/* Wait for a '\n' to show up in the input stream. If the line
	   is not complete, then keep waiting. */
      while ( (nl = strchr(chp->buf, '\n')) == 0 ) {
	    if (udelay == 0) {
		    /* User asked not to wait. */
		  break;
	    } else if (udelay < 0) {
		    /* User asked to wait forever... */
		  pthread_cond_wait(&chp->data_arrival, &chp->sync);
	    } else {
		    /* User asked to wait for a timeout... */
		  int prc = pthread_cond_timedwait(&chp->data_arrival, &chp->sync, &timeout);
		  if (prc<0 && errno==ETIMEDOUT)
			break;
	    }
      }

      if (nl && (*nl == '\n')) {
	    *nl = 0;
	    if (__libiseio_plug_log)
		  fprintf(__libiseio_plug_log, "ise_plug_readln: "
			  "GOT \"%s\"\n", chp->buf);

	    strncpy(buf, chp->buf, nbuf);
	    buf[nbuf-1] = 0;
	    rc = nl - chp->buf;

	    nl += 1;
	    if (nl-chp->buf >= chp->buf_fil) {
		  chp->buf_fil = 0;
	    } else {
		  memmove(chp->buf, nl, chp->buf_fil - (nl-chp->buf)+1);
		  chp->buf_fil -= nl - chp->buf;
	    }

      } else {
	      /* No data (timeout?) so set return values. */
	    buf[0] = 0;
	    rc = 0;

	    if (__libiseio_plug_log)
		  fprintf(__libiseio_plug_log, "ise_plug_readln: "
			  "Gave up on readln\n");
      }

      pthread_mutex_unlock(&chp->sync);
      return rc;
}

int ise_plug_writeln(int chn, const char*data)
{
      struct channel_data*chp = __libiseio_channels + chn;

      pthread_mutex_lock(&chp->sync);

      if (chp->fd >= 0) {
	    size_t len = strlen(data);
	    while (len > 0) {
		  int rc = write(chp->fd, data, len);
		  assert(rc >= 0);
		  data += rc;
		  len -= rc;
	    }

	    write(chp->fd, "\n", 1);
      }

      pthread_mutex_unlock(&chp->sync);
      return 0;
}
