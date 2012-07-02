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
# include  <fcntl.h>
# include  <errno.h>
# include  <sys/types.h>
# include  <sys/mman.h>
# include  <sys/socket.h>
# include  <sys/un.h>
# include  <assert.h>

/*
 * This is the code for handling the command channel communication
 * from the host. This includes opening channels, starting the
 * application, etc.
 */

static pthread_t app_thread;

/*
 * OPEN <id> <pipe-path>
 */
static void open_fun(int argc, const char*argv[])
{
      assert(argc >= 3);

      if (__libiseio_plug_log)
	    fprintf(__libiseio_plug_log, "command<OPEN>: <%s> <%s> <%s>\n",argv[0], argv[1], argv[2]);

      int chn = strtol(argv[1],0,10);
      if (chn < 0)
	    return;
      if (chn >= 256)
	    return;

      int fd = socket(PF_UNIX, SOCK_STREAM, 0);

      struct sockaddr_un addr;
      memset(&addr, 0, sizeof addr);

      assert(strlen(argv[2]) < sizeof addr.sun_path);
      addr.sun_family = AF_UNIX;
      strcpy(addr.sun_path, argv[2]);

      int rc = connect(fd, (const struct sockaddr*)&addr, sizeof addr);
      assert(rc >= 0);

	/* If there is already a fd for the channel, then close it and
	   replace with the new fd. */
      if (__libiseio_channels[chn].fd >= 0)
	    close(__libiseio_channels[chn].fd);

      __libiseio_channels[chn].fd = fd;
      __libiseio_channels[chn].buf_fil = 0;

      if (__libiseio_plug_log)
	    fprintf(__libiseio_plug_log, "command<OPEN>: Open complete, fd=%d\n", fd);
}

/*
 * CLOSE <id>
 */
static void close_fun(int argc, const char*argv[])
{
      assert(argc >= 2);

      int chn = strtol(argv[1],0,10);
      if (chn < 0)
	    return;
      if (chn >= 256)
	    return;
      if (__libiseio_channels[chn].fd < 0)
	    return;

      close(__libiseio_channels[chn].fd);
      __libiseio_channels[chn].fd = -1;
}

/*
 * FRAME <id> <size> <path>
 */
static void frame_fun(int argc, const char*argv[])
{
      assert(argc >= 4);

      if (__libiseio_plug_log)
	    fprintf(__libiseio_plug_log, "command<FRAME>: "
		    "%s %s %s\n", argv[1], argv[2], argv[3]);

      int frm = strtol(argv[1],0,10);
      size_t len = strtoul(argv[2],0,0);

      int fd = open(argv[3],O_RDWR,0);
      assert(fd >= 0);

      struct frame_data*fdp = __libiseio_plug_frames+frm;

      pthread_mutex_lock(&fdp->sync);
      if (fdp->base_data.base) {
	    munmap(fdp->base_data.base, fdp->base_data.size);
      }
      fdp->base_data.base = mmap(0, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
      if (fdp->base_data.base) {
	    fdp->base_data.size = len;
      } else {
	    fdp->base_data.size = 0;
      }

      pthread_mutex_unlock(&fdp->sync);

      close(fd);

      char resp[512];
      snprintf(resp, sizeof resp, "FRAME %d, %zu\n", frm, len);
      int rc = write(__libiseio_plug_isex, resp, strlen(resp));
      assert(rc == strlen(resp));
}

static void*run_thread(void*obj)
{
      ise_plug_application(0,0);
      return 0;
}

static void run_fun(int argc, const char*argv[])
{
      if (__libiseio_plug_log)
	    fprintf(__libiseio_plug_log, "command<RUN>: "
		    "Create application thread.\n");

      int rc = pthread_create(&app_thread, 0, run_thread, 0);
}

static struct command_table {
      const char*name;
      void (*fun) (int argc, const char*argv[]);
} commands[] = {
      { "CLOSE", close_fun },
      { "FRAME", frame_fun },
      { "OPEN",  open_fun  },
      { "RUN",   run_fun   },
      { 0, 0 }
};

void __libiseio_process_command(char*line)
{
      const char*cargv[1024];
      int cargc = 0;

      while (line[0] != 0) {
	    cargv[cargc] = line;
	    size_t len = strcspn(line, " \t\n\r\f");
	    if (len == 0) {
		  line += 1;
		  continue;
	    }

	    cargc += 1;
	    cargv[cargc] = 0;

	    if (line[len] == 0) {
		  line += len;
	    } else {
		  line[len] = 0;
		  line += len+1;
	    }
      }

      struct command_table*cp = commands;
      while (cp->name) {
	    if (strcmp(cargv[0], cp->name) == 0) {
		  cp->fun (cargc, cargv);
		  return;
	    }
	    cp += 1;
      }
}
