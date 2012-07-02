/*
 * Copyright (c) 2012 Picture Elements, Inc.
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
# include  <stdio.h>
# include  <stdlib.h>
# include  <string.h>
# include  <unistd.h>
# include  <errno.h>
# include  <sys/select.h>
# include  <sys/types.h>
# include  <sys/socket.h>
# include  <sys/un.h>
# include  <assert.h>

# define VERSION_STAMP "plugin-library-version"

FILE*__libiseio_plug_log = 0;

int __libiseio_plug_isex = -1;

static void process_port(int port_fd)
{
      static char buf[8*1024];
      size_t buf_fil = 0;

      int rc = read(port_fd, buf+buf_fil, sizeof buf - buf_fil - 1);
      if (rc <= 0)
	    return;

      buf[buf_fil+rc] = 0;
      buf_fil += rc;

      char*nl;

      while ( (nl = strchr(buf, '\n')) ) {

	    *nl++ = 0;
	    __libiseio_process_command(buf);

	    memmove(buf, nl, buf_fil - (nl-buf)+1);
	    buf_fil -= nl-buf;
      }
}

static void collect_channel_data(int chn)
{
      struct channel_data*chp = __libiseio_channels + chn;

      pthread_mutex_lock(&chp->sync);

      int rc = read(chp->fd, chp->buf+chp->buf_fil,
		    sizeof chp->buf - chp->buf_fil - 1);
      if (rc > 0) {
	    chp->buf[chp->buf_fil + rc] = 0;
	    chp->buf_fil += rc;
	    pthread_cond_broadcast(&chp->data_arrival);
      }

      pthread_mutex_unlock(&chp->sync);
}

/*
 * Process input from the 254 channel. This is the monitor port.
 */
static void process_254(void)
{
      struct channel_data*chp = __libiseio_channels + 254;
      char*nl;

      pthread_mutex_lock(&chp->sync);

      while ( (nl = strchr(chp->buf, '\n')) ) {
	    char res[1024];
	    char*line = chp->buf;
	    *nl++ = 0;

	    if (line[0] == 'i') {
		  snprintf(res, sizeof res, "%s\n>", VERSION_STAMP);

	    } else {
		  snprintf(res, sizeof res, ">");
	    }

	    int rc = write(chp->fd, res, strlen(res));

	    memmove(chp->buf, nl, chp->buf_fil - (nl-chp->buf)+1);
	    chp->buf_fil -= nl-chp->buf;
      }

      pthread_mutex_unlock(&chp->sync);
}

int ise_plug_main(int argc, char*argv[])
{
      int idx;
      char*debug_path = 0;
      if ( (debug_path = getenv("LIBISEIO_PLUG_LOG")) ) {
	    if (strcmp(debug_path,"-") == 0) {
		  __libiseio_plug_log = stdout;
	    } else {
		  __libiseio_plug_log = fopen(debug_path, "w+t");
	    }
      }

      if (__libiseio_plug_log) {
	    fprintf(__libiseio_plug_log, "ise_plug_main: "
		    "Start plugin\n");
	    setlinebuf(__libiseio_plug_log);
	    fflush(__libiseio_plug_log);
      }

      for (idx = 0 ; idx < 256 ; idx += 1) {
	    __libiseio_channels[idx].fd = -1;
	    __libiseio_channels[idx].buf_fil = 0;
	    pthread_mutex_init(&__libiseio_channels[idx].sync, 0);
	    pthread_cond_init(&__libiseio_channels[idx].data_arrival, 0);
      }

      for (idx = 0 ; idx < 16 ; idx += 1) {
	    struct frame_data*fdp = __libiseio_plug_frames+idx;
	    fdp->base_data.id   = idx;
	    fdp->base_data.base = 0;
	    fdp->base_data.size = 0;
	    pthread_mutex_init(& fdp->sync, 0);
      }

      for (idx = 1 ; idx < argc ; idx += 1) {
	    if (strncmp(argv[idx],"--port-fd=",10) == 0) {
		  __libiseio_plug_isex = strtol(argv[idx]+10,0,10);
	    }
      }

	/* Send a HELLO message to let server know that I am alive. */
      write(__libiseio_plug_isex, "HELLO\n", 6);


      for (;;) {
	    int nfds = __libiseio_plug_isex + 1;
	    fd_set readfds;
	    FD_ZERO(&readfds);

	    FD_SET(__libiseio_plug_isex, &readfds);
	    for (idx = 0 ; idx < 256 ; idx += 1) {
		  if (__libiseio_channels[idx].fd >= 0) {
			FD_SET(__libiseio_channels[idx].fd, &readfds);
			if (__libiseio_channels[idx].fd >= nfds)
			      nfds = __libiseio_channels[idx].fd+1;
		  }
	    }

	    int rc = select(nfds, &readfds, 0, 0, 0);
	    if (rc < 0 && errno==EINTR)
		  continue;
	    assert(rc >= 0);

	    if (FD_ISSET(__libiseio_plug_isex, &readfds))
		  process_port(__libiseio_plug_isex);

	    for (idx = 0 ; idx < 256 ; idx += 1) {
		  if (__libiseio_channels[idx].fd < 0)
			continue;
		  if (FD_ISSET(__libiseio_channels[idx].fd, &readfds)) {
			collect_channel_data(idx);
			if (idx == 254)
			      process_254();
		  }
	    }
      }

      return 0;
}
