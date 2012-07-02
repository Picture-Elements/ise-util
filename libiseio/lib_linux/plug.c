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

# include  <libiseio.h>
# include  "priv.h"
# include  <sys/types.h>
# include  <sys/mman.h>
# include  <sys/socket.h>
# include  <sys/un.h>
# include  <unistd.h>
# include  <fcntl.h>
# include  <string.h>
# include  <errno.h>
# include  <assert.h>

/*
 * The plugin protocol communicates with a process linked using the
 * libiseio_plug library. The connect function creates a socket pair
 * as a control channel, and executes the process with the fd number
 * to the pipe on its command line. The new slave process writes a
 * "HELLO\n" string. At that point, the plug interface is connected to
 * the process.
 *
 * Commands through the control channel are ASCII strings terminated
 * by a newline character. The process responds be executing the
 * command or responding with results, as appropriate. The defined
 * commands are:
 *
 *   OPEN <id> <path>
 *     Open a channel. The master creates a named pipe to represent
 *     the channel, then sends it as the <path> argument to the open
 *     command. The slave responds by connecting to the port. The
 *     master detects the connection as the completion of the open
 *     command.
 *
 *   CLOSE <id>
 *     Close the channel.
 */
//# define ISEIO_VAR_PIPES "/var/iseio/plug"

/*
 * This is the implementation of libiseio that implements local
 * plugins on Linux.
 */

static const char*plug_name(struct ise_handle*dev)
{
      return dev->id_str+5;
}

static int probe_id_plug(struct ise_handle*dev)
{
      if (strncmp(dev->id_str,"plug:",5) != 0)
	    return 0;
      if (dev->id_str[5] == 0)
	    return 0;

      return 1;
}

static int readln(int fd, char*buf, size_t nbuf)
{
      char*cp = buf;
      while (nbuf > 1) {
	    int rc = read(fd, cp, 1);
	    assert(rc > 0);
	    if (*cp == '\n') {
		  *cp = 0;
		  return cp - buf;
	    }

	    cp += 1;
	    nbuf -= 1;
      }

      *cp = 0;
      return cp - buf;
}

/*
 * In the plugin device, the connect function executes the plugin
 * object and connects the command channel.
 */
static ise_error_t connect_plug(struct ise_handle*dev)
{
      int rc;
      pid_t pid;
      char buf[64];

	/* Locate the plugin file. */
      char path[4096];
      snprintf(path, sizeof path, "./%s.plg", plug_name(dev));

      if (access(path, X_OK) < 0) {
	    if (__ise_logfile)
		  fprintf(__ise_logfile, "Plugin %s is not executable.\n", path);
	    return ISE_ERROR;
      }

	/* Create a socket pair for use communicating with the plugin. */
      int sv[2];
      rc = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
      assert(rc >= 0);

	/* Fork a child process! */
      pid = fork();
      if (pid < 0)
	    return ISE_ERROR;

	/* The child process executes the plugin binary. */
      if (pid == 0) {
	    char port_str[16];
	    snprintf(port_str, sizeof port_str, "--port-fd=%d", sv[1]);
	    close(sv[0]);
	      /* Detach stdin */
	    close(0);
	    open("/dev/null", O_RDONLY, 0);
	      /* Execute the process */
	    execl(path, path, port_str, 0);
	    assert(0);
      }

	/* Parent process... */

	/* The client side of the pipe is not for me. The parent side
	   is the isex file. */
      close(sv[1]);
      dev->isex = sv[0];

	/* Wait for the HELLO message from the child. This indicates
	   that it is ready. */
      buf[0] = 0;
      while (strcmp(buf,"HELLO") != 0) {
	    readln(dev->isex, buf, sizeof buf);
	    assert(rc >= 0);
	    if (__ise_logfile) {
		  fprintf(__ise_logfile, "%s: Got message %s from plugin\n",
			  dev->id_str, buf);
	    }
      }

      return ISE_OK;
}

static ise_error_t restart_plug(struct ise_handle*dev)
{
      if (__ise_logfile) {
	    fprintf(__ise_logfile, "%s: restart not implemented\n", dev->id_str);
      }

      return ISE_ERROR;
}

static ise_error_t run_program_plug(struct ise_handle*dev)
{
      if (__ise_logfile) {
	    fprintf(__ise_logfile, "%s: run_program\n", dev->id_str);
      }

      char buf[32];
      snprintf(buf, sizeof buf, "RUN\n");

      int rc = write(dev->isex, buf, strlen(buf));
      assert(rc == strlen(buf));

      return ISE_OK;
}

static ise_error_t channel_open_plug(struct ise_handle*dev,
				     struct ise_channel*chn)
{
      struct sockaddr_un addr;
      memset(&addr, 0, sizeof addr);

      addr.sun_family = AF_UNIX;
      snprintf(addr.sun_path, sizeof addr.sun_path,
	       "%s/%s.%u", ISEIO_VAR_PIPES,
	       dev->id_str, chn->cid);

      if (__ise_logfile) {
	    fprintf(__ise_logfile, "%s: channel_open channel=%u, pipe=%s\n",
		    dev->id_str, chn->cid, addr.sun_path);
      }


      int fd = socket(PF_UNIX, SOCK_STREAM, 0);
      assert(fd >= 0);

      int rc = bind(fd, (const struct sockaddr*)&addr, sizeof addr);
      if (rc < 0) {
	    switch (errno) {
		default:
		  fprintf(stderr, "%s: Unable to bind to pipe %s (errno=%d)\n",
			  dev->id_str, addr.sun_path);
		  break;
	    }
	    close(fd);
	    return ISE_ERROR;
      }

      rc = listen(fd, 2);

	/* Tell the remote to connect to this channel */
      char buf[32 + sizeof addr.sun_path];
      snprintf(buf, sizeof buf, "OPEN %u %s\n", chn->cid, addr.sun_path);
      rc = write(dev->isex, buf, strlen(buf));
      assert(rc == strlen(buf));

	/* Accept the connection from the remote. Close and unlink the
	   named pipe, and leave the client pipe open. */
      struct sockaddr remote_addr;
      socklen_t remote_addr_len;
      int use_fd = accept(fd, &remote_addr, &remote_addr_len);

      close(fd);
      unlink(addr.sun_path);

      chn->fd = use_fd;

      return ISE_OK;
}

static ise_error_t channel_sync_plug(struct ise_handle*dev,
				     struct ise_channel*chn)
{
      if (__ise_logfile) {
	    fprintf(__ise_logfile, "%s: channel_sync not implemented\n", dev->id_str);
      }

      return ISE_ERROR;
}

static ise_error_t channel_close_plug(struct ise_handle*dev,
				      struct ise_channel*chn)
{
      if (__ise_logfile) {
	    fprintf(__ise_logfile, "%s: Close channel %u\n", dev->id_str, chn->cid);
      }

      char buf[32];
      snprintf(buf, sizeof buf, "CLOSE %u\n", chn->cid);

      int rc = write(dev->isex, buf, strlen(buf));
      assert(rc == strlen(buf));

      close(chn->fd);
      chn->fd = -1;

      return ISE_OK;
}

static ise_error_t timeout_plug(struct ise_handle*dev, unsigned cid,
				long read_timeout)
{
      if (__ise_logfile) {
	    fprintf(__ise_logfile, "%s: timeout not implemented\n", dev->id_str);
      }

      return ISE_ERROR;
}

static ise_error_t make_frame_plug(struct ise_handle*dev, unsigned id)
{
      if (__ise_logfile) {
	    fprintf(__ise_logfile, "%s: make_frame not implemented\n", dev->id_str);
      }

      char path[4096];
      snprintf(path, sizeof path, "%s/%s.frame%u", ISEIO_VAR_PIPES,
	       dev->id_str, id);

      int fd = open(path, O_RDWR|O_CREAT, 0600);
      if (fd < 0)
	    return ISE_ERROR;

      int rc = ftruncate(fd, dev->frame[id].size);
      assert(rc >= 0);

      dev->frame[id].base = mmap(0, dev->frame[id].size,
				 PROT_READ|PROT_WRITE,
				 MAP_SHARED, fd, 0);
      assert(dev->frame[id].base);

      char buf[32 + sizeof path];
      snprintf(buf, sizeof buf, "FRAME %u %zu %s\n", id, dev->frame[id].size, path);
      rc = write(dev->isex, buf, strlen(buf));
      assert(rc == strlen(buf));

      rc = read(dev->isex, buf, sizeof buf);
      assert(rc >= 0);

      if (__ise_logfile)
	    fprintf(__ise_logfile, "%s: make_frame got %s from plugin.\n",
		    dev->id_str, buf);

	/* The mapping persists even though we close the fd and unlink
	   the path. These steps prevent the frame getting accessed by
	   anyone else. */
      close(fd);
      unlink(path);

      return ISE_OK;
}

static void delete_frame_plug(struct ise_handle*dev, unsigned id)
{
      if (id >= 16)
	    return;

      if (dev->frame[id].base == 0)
	    return;

      munmap(dev->frame[id].base, dev->frame[id].size);

      dev->frame[id].base = 0;
      dev->frame[id].size = 0;
}

static ise_error_t write_plug(struct ise_handle*dev,
			      struct ise_channel*chn,
			      const void*buf, size_t nbuf)
{
      write(chn->fd, buf, nbuf);
      return ISE_OK;
}

static ise_error_t writeln_plug(struct ise_handle*dev,
				struct ise_channel*chn,
				const char*text)
{
      int rc;
      char nl;
      rc = write(chn->fd, text, strlen(text));
      assert(rc == strlen(text));

      if (__ise_logfile)
	    fprintf(__ise_logfile, "%s.%u: write returns rc=%d\n",
		    dev->id_str, chn->cid, rc);

      nl = '\n';
      rc = write(chn->fd, &nl, 1);
      assert(rc == 1);

      return ISE_OK;
}

static ise_error_t readbuf_plug(struct ise_handle*dev,
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
		  fprintf(__ise_logfile, "%s.%u: read errorno=%d\n",
			  dev->id_str, chn->cid, errno);
		  fflush(__ise_logfile);
	    }

	    return ISE_ERROR;
      }

      chn->ptr = 0;
      chn->fill = rc;
      return ISE_OK;
}

const struct ise_driver_functions __driver_plug = {
 probe_id: probe_id_plug,
 connect: connect_plug,
 restart: restart_plug,
 run_program: run_program_plug,

 channel_open: channel_open_plug,
 channel_close: channel_close_plug,
 channel_sync: channel_sync_plug,
 timeout: timeout_plug,

 make_frame:  make_frame_plug,
 delete_frame: delete_frame_plug,

 write: write_plug,
 writeln: writeln_plug,
 readbuf: readbuf_plug,
};
