/*
 * Copyright (c) 1997 Picture Elements, Inc.
 *    Stephen Williams
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
#ifndef WIN32
#ident "$Id: ucrpipe.c,v 1.1 2001/08/28 23:39:05 steve Exp $"
#endif
/*
 * $Log: ucrpipe.c,v $
 * Revision 1.1  2001/08/28 23:39:05  steve
 *  Move ucrpipe command to ise product.
 *
 * Revision 1.8  1998/07/11 22:53:26  steve
 *  More friendly board select.
 *
 * Revision 1.7  1998/06/23 01:47:01  steve
 *  Add sync when needed.
 *
 * Revision 1.6  1998/04/22 19:12:26  steve
 *  Getting stuck on PeekConsoleInput.
 *
 * Revision 1.5  1998/04/18 18:46:17  steve
 *  A little less prone to hanging on console.
 *
 * Revision 1.4  1998/04/18 18:00:35  steve
 *  NT port of ucrpipe
 *
 * Revision 1.3  1998/04/07 18:42:36  steve
 *  Use partial reads.
 *
 * Revision 1.2  1997/10/24 22:30:37  steve
 *  Fix error message.
 *
 * Revision 1.1  1997/09/02 22:52:24  steve
 *  Add the ucrpipe program to the ucr module.
 */

/*
 * This program supports piping information into a UCR device and back
 * out using the ucrif device driver.
 */

# include  <ucrif.h>
# include  <stdio.h>
# include  <string.h>

# define DEFAULT_ISE "ise0"

#ifdef WIN32
/* This is NT versions of the channel operations. */
# include  <windows.h>
# include  <io.h>

# define ISE_PATH_FORMAT "\\\\.\\%s"

static HANDLE open_chan(const char*path, unsigned chan)
{
      BOOL rc;
      DWORD junk;
      OVERLAPPED over;
      HANDLE fd;

      fd = CreateFile(path, GENERIC_READ|GENERIC_WRITE,
		      FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING,
		      FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED, 0);
      if (fd == INVALID_HANDLE_VALUE) {
	    fprintf(stderr, "Failed to open ``%s''\n", path);
	    return INVALID_HANDLE_VALUE;
      }

      over.hEvent = CreateEvent(0, TRUE, FALSE, 0);

      over.Offset = 0;
      over.OffsetHigh = 0;
      ResetEvent(over.hEvent);

      rc = DeviceIoControl(fd, UCR_CHANNEL, &chan,
			   sizeof chan,  0, 0, 0, &over);
      GetOverlappedResult(fd, &over, &junk, TRUE);

      CloseHandle(over.hEvent);

      return fd;
}

int write_chan(HANDLE fd, const char*buffer, size_t cnt)
{
      long out;
      OVERLAPPED over;

      over.Offset = 0;
      over.OffsetHigh = 0;
      over.hEvent = CreateEvent(0, TRUE, FALSE, 0);

      ResetEvent(over.hEvent);
      WriteFile(fd, buffer, cnt, &out, &over);
      WaitForSingleObject(over.hEvent, INFINITE);

      CloseHandle(over.hEvent);

      return out;
}

static int flush_chan(HANDLE fd, int file_mark_flag, int sync_flag)
{
      BOOL rc;
      DWORD junk;
      OVERLAPPED over;

      over.hEvent = CreateEvent(0, TRUE, FALSE, 0);

      over.Offset = 0;
      over.OffsetHigh = 0;
      ResetEvent(over.hEvent);

      rc = DeviceIoControl(fd, sync_flag?UCR_SYNC:UCR_FLUSH, 0, 0,  0,
			   0, 0, &over);
      GetOverlappedResult(fd, &over, &junk, TRUE);

      if (file_mark_flag) {
	    over.Offset = 0;
	    over.OffsetHigh = 0;
	    ResetEvent(over.hEvent);

	    rc = DeviceIoControl(fd, UCR_SEND_FILE_MARK, 0,
				 0,  0, 0, 0, &over);
	    GetOverlappedResult(fd, &over, &junk, TRUE);
      }

      CloseHandle(over.hEvent);

      return 0;
}

/* XXXX
 * All this is very nice in theory, but it doesn't seem to work. I
 * just can't seem to figure out how to do asynchronous reads on a tty
 * window (otherwise known as CONIN$) under NT. The problem is not
 * being able to wait for something interesting to happen on the
 * console. The problem is that the console is signalled readable even
 * when the line is not ready.
 *
 * The only way to make this work is to work in RAW mode and take care
 * of handling keycodes, etc. Yechk!
 */
static int check_for_console_input(HANDLE coni)
{
      BOOL rc;
      INPUT_RECORD input;
      DWORD out;

      for (;;) {
	    rc = GetNumberOfConsoleInputEvents(coni, &out);
	    if (out == 0)
		  return 0;

	    rc = PeekConsoleInput(coni, &input, 1, &out);
	    if (!rc)
		  return 0;


	    if (input.EventType == MENU_EVENT) {
		  ReadConsoleInput(coni, &input, 1, &out);
		  continue;
	    }

	    if (input.EventType == FOCUS_EVENT) {
		  ReadConsoleInput(coni, &input, 1, &out);
		  continue;
	    }

	    if (input.EventType == MOUSE_EVENT) {
		  ReadConsoleInput(coni, &input, 1, &out);
		  continue;
	    }

	    if (input.EventType == WINDOW_BUFFER_SIZE_EVENT) {
		  ReadConsoleInput(coni, &input, 1, &out);
		  continue;
	    }


	    return 1;
      }
}

static int interact(HANDLE fd, int file_mark_flag)
{
      HANDLE cons = GetStdHandle(STD_INPUT_HANDLE);
      HANDLE cono = GetStdHandle(STD_OUTPUT_HANDLE);
      HANDLE syncs[2];
      OVERLAPPED over;

      char cbuf[128];
      char ubuf[128];

      syncs[0] = cons;
      syncs[1] = CreateEvent(0, TRUE, FALSE, 0);

      over.hEvent = syncs[1];
      over.Offset = 0;
      over.OffsetHigh = 0;

      SetConsoleMode(cons, ENABLE_LINE_INPUT
		           |ENABLE_ECHO_INPUT
		           |ENABLE_PROCESSED_INPUT);
      ResetEvent(syncs[1]);
      ReadFile(fd, ubuf, sizeof ubuf, 0, &over);
      for (;;) {
	    long out;
	    DWORD rc = WaitForMultipleObjectsEx(2, syncs, FALSE,
						INFINITE, FALSE);

	    switch (rc) {

		case 0:
		  if (! check_for_console_input(cons))
			break;
		  ReadFile(cons, cbuf, sizeof cbuf, &out, 0);
		  cbuf[out] = 0;
		  if (out > 0) {
			write_chan(fd, cbuf, out);
			flush_chan(fd, 0, 0);
		  }
		  break;

		case 1:
		  GetOverlappedResult(fd, &over, &out, TRUE);
		  SetConsoleTextAttribute(cono, FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY);
		  WriteFile(cono, ubuf, out, &out, 0);
		  SetConsoleTextAttribute(cono, FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE);

		  ResetEvent(over.hEvent);
		  over.Offset = 0;
		  over.OffsetHigh = 0;
		  ReadFile(fd, ubuf, sizeof ubuf, 0, &over);
		  break;

		default:
		  fprintf(stderr, "strange rc=%d from wait\n", rc);
		  return -1;
	    }

      }

      CloseHandle(syncs[1]);
      return 0;
}

#else
/* These are UNIX versions of the various channel functions. */
# include  <sys/time.h>
# include  <sys/types.h>
# include  <sys/stat.h>
# include  <fcntl.h>
# include  <unistd.h>

# define ISE_PATH_FORMAT "/dev/%s"

#define HANDLE int
#define INVALID_HANDLE_VALUE -1
static int open_chan(const char*path, unsigned chan)
{
      int rc;
      int fd = open(path, O_RDWR, 0);
      if (fd == -1) {
	    perror(path);
	    return INVALID_HANDLE_VALUE;
      }

      rc = ioctl(fd, UCR_CHANNEL, chan);
      if (rc == -1) {
	    fprintf(stderr, "%s: Error setting channel to %u\n",
		    path, chan);
	    return INVALID_HANDLE_VALUE;
      }

      return fd;
}

static inline int write_chan(HANDLE fd, const char*buffer, size_t cnt)
{
      return write(fd, buffer, cnt);
}

static inline int flush_chan(HANDLE fd, int file_mark_flag, int sync_flag)
{
      int rc = ioctl(fd, sync_flag?UCR_SYNC:UCR_FLUSH, 0);
      if (file_mark_flag) {
	    rc = ioctl(fd, UCR_SEND_FILE_MARK, 0);
      }
      return rc;
}

static int interact(HANDLE fd, int file_mark_flag)
{
      char buf[128];
      int rc;
      fd_set read_set;

      for (;;) {
	    FD_ZERO(&read_set);
	    FD_SET(0, &read_set);
	    FD_SET(fd, &read_set);

	    rc = select(fd+1, &read_set, 0, 0, 0);
	    if (FD_ISSET(0, &read_set))
		  switch ( (rc = read(0, buf, sizeof buf)) ) {

		      case -1:
			perror("stdin");
			return 1;

		      case 0:
			if (file_mark_flag) {
			      fprintf(stderr, "Writing file mark.\n");
			      rc = ioctl(fd, UCR_SEND_FILE_MARK, 0);
			}
			return 0;

		      default:
			rc = write(fd, buf, rc);
			ioctl(fd, UCR_FLUSH, 0);
		  }

	    if (FD_ISSET(fd, &read_set)) {
		  rc = read(fd, buf, sizeof buf);
		  switch (rc) {
		      case -1:
			perror("target");
			return 1;

		      default:
			write(1, buf, rc);
		  }
	    }
      }
}

#endif


main (int argc, char*argv[])
{
      HANDLE fd;
      int rc;
      int idx;
      int chan = 1;
      const char*ise = DEFAULT_ISE;
      char fmt[128];

      int file_mark_flag = 0;

      for (idx = 1 ;  idx < argc ;  idx += 1) {

	    if (argv[idx][0] != '-')
		  break;

	    switch (argv[idx][1]) {

		case 'c':
		  idx += 1;
		  if (idx == argc) {
			fprintf(stderr, "missing value for -c\n");
			return -1;
		  }
		  chan = strtoul(argv[idx],0,0);
		  break;

		case 'f':
		  file_mark_flag = 0;
		  break;

		case 'F':
		  file_mark_flag = 1;
		  break;

		case 'p':
		  idx += 1;
		  if (idx == argc) {
			fprintf(stderr, "missing value for -p\n");
			return -1;
		  }
		  ise = argv[idx];
		  break;

		default:
		  fprintf(stderr, "unknown switch %s\n", argv[idx]);
		  return -1;
	    }
      }

      sprintf(fmt, ISE_PATH_FORMAT, ise);
      fd = open_chan(fmt, chan);
      if (fd == INVALID_HANDLE_VALUE)
	    return -1;

	/* If there are files on the command line, then dump those
	   files into the channel. Do not go on to the interactive
	   behavior below. */
      if (idx < argc) {
	    while (idx < argc) {
		  char buffer[1024];
		  FILE*file = fopen(argv[idx], "rb");
		  size_t cnt;

		  if (file == 0) {
			fprintf(stderr, "Unable to open %s for reading\n",
				argv[idx]);
			return -3;
		  }

		  do {
			cnt = fread(buffer, 1, sizeof buffer, file);
			rc = write_chan(fd, buffer, cnt);
		  } while (cnt == sizeof buffer);

		  fclose(file);
		  idx += 1;
	    }

	    rc = flush_chan(fd, file_mark_flag, 1);

	    return 0;
      }

	/* If I got through the previous test, there are no file names
	   on the command line and I go into interactive mode. This
	   involves watching stdin *and* the channel for input, and
	   passing the values from one to the other. */

      return interact(fd, file_mark_flag);
}
