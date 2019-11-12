/*
 * Copyright (c) 2000-2002 Picture Elements, Inc.
 *    Stephen Williams (steve@picturel.com)
 */

# include  <libiseio.h>
# include  <ucrif.h>
# include  <stdlib.h>
# include  <stdio.h>
# include  <string.h>
# include  <windows.h>

/*
 * This registry entry contains the path to the directory that holds
 * ISE firmware .scof files. The ise_restart function uses that
 * directory to locate firmware here if it is not in the current
 * working directory.
 */
static const char*rkey = "System\\CurrentControlSet\\Services\\ise";

static FILE*lib_logfile = 0;

struct ise_channel {
      struct ise_channel*next;
      unsigned cid;
      HANDLE fd;
      char buf[1024];
      unsigned ptr, fill;
};

struct ise_handle {
      unsigned id;
      HANDLE isex;
      char*version;
      struct ise_channel*clist;

      HANDLE frame_isex;
      struct {
	    OVERLAPPED over;
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


ise_error_t ise_channel(struct ise_handle*dev, unsigned cid)
{
      struct ise_channel*chn;
      HANDLE fd;
      BOOL rc;
      OVERLAPPED over;

	/* Create the HANDLE for the channel to support overlapped
	   I/O. Need this so that multiple threads can access the
	   channel */
      { char path[32];
        sprintf(path, "\\\\.\\ise%u", dev->id);
	fd = CreateFile(path, GENERIC_READ|GENERIC_WRITE, 0, 0,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED, 0);
      }

      if (fd == INVALID_HANDLE_VALUE) {
	    return ISE_ERROR;
      }

      over.hEvent = CreateEvent(0,TRUE,0,0);
      ResetEvent(over.hEvent);

      { unsigned chan = cid;
        DWORD junk;
        rc = DeviceIoControl(fd, UCR_CHANNEL, &chan, sizeof chan,
			     0, 0, &junk, &over);
	if ((rc == 0) && (GetLastError() == ERROR_IO_PENDING))
	      rc = GetOverlappedResult(fd, &over, &junk, TRUE);

	if (! rc) {
	      if (lib_logfile) {
		    fprintf(lib_logfile, "ise%u: ise_channel(%d) failed.\n",
			    dev->id, cid);
		    fflush(lib_logfile);
	      }

	      CloseHandle(over.hEvent);
	      CloseHandle(fd);
	      return ISE_CHANNEL_BUSY;
	}
      }

      CloseHandle(over.hEvent);

      chn = calloc(1, sizeof (struct ise_channel));
      chn->cid = cid;
      chn->fd  = fd;
      chn->ptr = 0;
      chn->fill = 0;
      chn->next = dev->clist;
      dev->clist = chn;

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u: ise_channel(%d) success.\n",
		    dev->id, cid);
	    fflush(lib_logfile);
      }

      return ISE_OK;
}

/*
 * Making a frame involves actually making the frame, and mapping it
 * into the address space of this process. If the frame already
 * exists, then map that. The size of the mapped result is returned in
 * the *siz value.
 */
void* ise_make_frame(struct ise_handle*dev, unsigned id, size_t*siz)
{
      struct IsexMmapInfo frame_arg;
      DWORD junk;
      BOOL rc;

      *siz = (*siz + 4095) & ~4095;

      if (dev->frame[id].base) {
	    *siz = dev->frame[id].size;
	    return dev->frame[id].base;
      }

      if (dev->clist == 0)
	    return 0;

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u: ise_make_frame(%u, %08x).\n",
		    dev->id, id, *siz);
	    fflush(lib_logfile);
      }


      dev->frame[id].size = *siz;
      dev->frame[id].base = VirtualAlloc(0, *siz, MEM_COMMIT, PAGE_READWRITE);

      frame_arg.frame_id = id;

      dev->frame[id].over.hEvent = CreateEvent(0,TRUE,0,0);
      ResetEvent(dev->frame[id].over.hEvent);
      rc = DeviceIoControl(dev->frame_isex, ISEX_MAKE_MAP_FRAME,
			   &frame_arg, sizeof frame_arg,
			   dev->frame[id].base, dev->frame[id].size,
			   0, &dev->frame[id].over);
      if (rc != 0) {
      } else switch (GetLastError()) {

	  case ERROR_IO_PENDING:
	    break;

	  default:
	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u: ise_make_frame MAP ioctl "
			  "return error 0x%08lx\n", dev->id, GetLastError());
		  fflush(lib_logfile);
	    }
	    VirtualFree(dev->frame[id].base, 0, MEM_RELEASE);
	    dev->frame[id].base = 0;
	    dev->frame[id].size = 0;
	    return 0;
      }

      rc = DeviceIoControl(dev->isex, ISEX_WAIT_MAP_FRAME,
			   &frame_arg, sizeof frame_arg,
			   0, 0, &junk, 0);
      if (rc == 0)  {
	    long err = GetLastError();
	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u: ise_make_frame MAP WAIT ioctl "
			  "return error 0x%08lx\n", dev->id, err);
		  fflush(lib_logfile);
	    }
	    VirtualFree(dev->frame[id].base, 0, MEM_RELEASE);
	    dev->frame[id].base = 0;
	    dev->frame[id].size = 0;
	    return 0;
      }

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u: Frame mapped to %p.\n",
		    dev->id, dev->frame[id].base);
	    fflush(lib_logfile);
      }

      *siz = dev->frame[id].size;
      return dev->frame[id].base;
}

void ise_delete_frame(struct ise_handle*dev, unsigned id)
{
      DWORD junk;
      BOOL rc;
      OVERLAPPED over;
      unsigned long arg;
      struct IsexMmapInfo frame_arg;

      if (id >= 16)
	    return;

      if (dev->frame[id].base == 0)
	    return;

      frame_arg.frame_id = id;

      rc = DeviceIoControl(dev->isex, ISEX_UNMAP_UNMAKE_FRAME,
			   &frame_arg, sizeof frame_arg,
			   0, 0, &junk, 0);
      if (rc == 0)  {
	    long err = GetLastError();
	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u: ise_delete_frame UNMAP ioctl "
			  "return error 0x%08lx\n", dev->id, err);
		  fflush(lib_logfile);
	    }
      }

      rc = GetOverlappedResult(dev->frame_isex,
			       &dev->frame[id].over,
			       &junk, TRUE);
      if (! rc) {
	    if (lib_logfile) {
		  long err = GetLastError();
		  fprintf(lib_logfile, "ise%u: ise_delete_frame MAP ioctl "
			  "return error 0x%08lx\n", dev->id, err);
		  fflush(lib_logfile);
	    }
      }

      CloseHandle(dev->frame[id].over.hEvent);
      dev->frame[id].over.hEvent = 0;

      VirtualFree(dev->frame[id].base, 0, MEM_RELEASE);
      dev->frame[id].base = 0;
      dev->frame[id].size = 0;
}


ise_error_t ise_writeln(struct ise_handle*dev, unsigned cid,
			const char*text)
{
      struct ise_channel*chn;
      BOOL rc;
      OVERLAPPED over;
      DWORD cnt;

      chn = dev->clist;
      while (chn && (chn->cid != cid))
	    chn = chn->next;

      if (chn == 0)
	    return ISE_NO_CHANNEL;

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u.%u: writeln text(%s)\n",
		    dev->id, cid, text);
	    fflush(lib_logfile);
      }

      over.hEvent = CreateEvent(0,TRUE,0,0);
      over.Offset = 0;
      over.OffsetHigh = 0;

      rc = WriteFile(chn->fd, text, strlen(text), &cnt, &over);
      if ((rc == FALSE) && (GetLastError() == ERROR_IO_PENDING))
	    rc = GetOverlappedResult(chn->fd, &over, &cnt, TRUE);

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u.%u: wrote %d(%d) bytes, writing EOL\n",
		    dev->id, cid, rc ? cnt : -1, GetLastError());
	    fflush(lib_logfile);
      }

      over.Offset = 0;
      over.OffsetHigh = 0;

      { char nl = '\n';
        rc = WriteFile(chn->fd, &nl, 1, &cnt, &over);
	if ((rc == FALSE) && (GetLastError() == ERROR_IO_PENDING))
	      rc = GetOverlappedResult(chn->fd, &over, &cnt, TRUE);
      }

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u.%u: flushing written data.\n",
		    dev->id, cid);
	    fflush(lib_logfile);
      }

      ResetEvent(over.hEvent);

      rc = DeviceIoControl(chn->fd, UCR_FLUSH, 0, 0, 0, 0, &cnt, &over);
      if ((rc == FALSE) && (GetLastError() == ERROR_IO_PENDING))
	    rc = GetOverlappedResult(chn->fd, &over, &cnt, TRUE);

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u.%u: ise_writeln done.\n",
		    dev->id, cid);
	    fflush(lib_logfile);
      }

      CloseHandle(over.hEvent);

      return ISE_OK;
}

ise_error_t ise_readln(struct ise_handle*dev, unsigned cid,
		       char*buf, size_t nbuf)
{
      struct ise_channel*chn;
      BOOL rc;
      OVERLAPPED over;
      DWORD cnt;
      char*bp;

      chn = dev->clist;
      while (chn && (chn->cid != cid))
	    chn = chn->next;

      if (chn == 0)
	    return ISE_NO_CHANNEL;


      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u.%u: ise_readln start.\n",
		    dev->id, cid);
	    fflush(lib_logfile);
      }

      over.hEvent = CreateEvent(0,TRUE,0,0);

      bp = buf;
      do {
	    while (chn->fill > 0) {
		  *bp = chn->buf[chn->ptr];
		  chn->ptr += 1;
		  chn->fill -= 1;

		  if ((*bp == '\n') && (strncmp(buf,"<\001>",3) == 0)) {

			  /* Detect lines that are meant to be logged
			     but not passed to the application. */

			*bp = 0;
			if (lib_logfile) {
			      fprintf(lib_logfile, "ise%u.%u: "
				      "ise_readln log: %s\n",
				      dev->id, cid, buf+3);
			      fflush(lib_logfile);
			}
			bp = buf;
			*bp = 0;

		  } else if (*bp == '\n') {
			  /* Detect the line end, for a line that can
			     be sent back to the application. */

			*bp = 0;
			CloseHandle(over.hEvent);
			if (lib_logfile) {
			      fprintf(lib_logfile, "ise%u.%u: "
				      "ise_readln returns: %s\n",
				      dev->id, cid, buf);
			      fflush(lib_logfile);
			}
			return ISE_OK;

		  } else {
			bp += 1;
		  }
	    }

	    over.Offset = 0;
	    over.OffsetHigh = 0;

	    rc = ReadFile(chn->fd, chn->buf, sizeof chn->buf, &cnt, &over);
	    if ((rc == 0) && (GetLastError() == ERROR_IO_PENDING))
		  rc = GetOverlappedResult(chn->fd, &over, &cnt, TRUE);
	    if (! rc) {
		  *bp = 0;
		  CloseHandle(over.hEvent);
		  return ISE_ERROR;
	    }

	      /* If there was a timeout, either timed or forced, then
		 the read count will be 0. Terminate whatever I got
		 and return an ISE_TIMEOUT error code. */
	    if (cnt == 0) {
		  *bp = 0;
		  CloseHandle(over.hEvent);
		  return ISE_CHANNEL_TIMEOUT;
	    }

	    if (lib_logfile) {
		  chn->buf[cnt] = 0;
		  fprintf(lib_logfile, "ise%u.%u: ise_readln: ReadFile: %s\n",
			  dev->id, cid, chn->buf);
		  fflush(lib_logfile);
	    }

	    chn->ptr = 0;
	    chn->fill = cnt;

      } while (bp < (buf+nbuf));

      CloseHandle(over.hEvent);
      return ISE_ERROR;
}

const char*ise_prom_version(struct ise_handle*dev)
{
      return dev->version;
}

ise_error_t ise_restart(struct ise_handle*dev, const char*firm)
{
      char path[4096];
      BOOL rc;
      DWORD cnt;
      HANDLE ch0, fd;
      unsigned wait_count;

      if (! dev->version) {
	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u: ****  ise_restart(<%u>,%s)"
			  " not allowed on ise_bind handles.\n",
			  dev->id, dev->id, firm);
		  fflush(lib_logfile);
	    }
	    return ISE_ERROR;
      }

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u: **** ise_restart(<%u>,%s)\n",
		    dev->id, dev->id, firm);
	    fflush(lib_logfile);
      }

      rc = DeviceIoControl(dev->isex, UCRX_RESET_BOARD, 0, 0, 0, 0,
			   &cnt, NULL);
      if (!rc)
	    rc = DeviceIoControl(dev->isex, UCRX_RESTART_BOARD, 0, 0, 0, 0,
				 &cnt, NULL);
      if (! rc) 
	    return ISE_ERROR;


	/* Open channel 0 to the firmware. This will be where I shove
	   the firmware. */

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u: open channel 0\n", dev->id);
	    fflush(lib_logfile);
      }

      sprintf(path, "\\\\.\\ise%u", dev->id);

      ch0 = CreateFile(path, GENERIC_READ|GENERIC_WRITE, 0, 0,
		       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
      if (ch0 == INVALID_HANDLE_VALUE)
	    return ISE_ERROR;


	/* Open the firmware .scof file. */

      sprintf(path, "%s.scof", firm);
      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u: try firmware %s\n",
		    dev->id, path);
	    fflush(lib_logfile);
      }

      fd = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 0,
		      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
      if (fd == INVALID_HANDLE_VALUE) {
	    DWORD junk;
	    HKEY regist;
	    long lrc;
	    char*cp;

	    lrc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, rkey, 0, KEY_READ, &regist);
	    if (lrc != ERROR_SUCCESS) {
		  CloseHandle(ch0);
		  return ISE_NO_SCOF;
	    }

	    cnt = sizeof path;
	    lrc = RegQueryValueEx(regist, "Firmware", 0, &junk, path, &cnt);
	    if (lrc != ERROR_SUCCESS) {
		  CloseHandle(ch0);
		  return ISE_NO_SCOF;
	    }

	    cp = path + strlen(path);
	    sprintf(cp, "\\%s.scof", firm);


	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u: try firmware %s\n",
			  dev->id, path);
		  fflush(lib_logfile);
	    }

	    fd = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 0,
			    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	    if (fd == INVALID_HANDLE_VALUE) {
		  if (lib_logfile) {
			fprintf(lib_logfile, "ise%u: **** No firmware, "
				"giving up.\n", dev->id, path);
			fflush(lib_logfile);
		  }

		  CloseHandle(ch0);
		  return ISE_NO_SCOF;
	    }
      }

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u: transmitting firmware\n", dev->id);
	    fflush(lib_logfile);
      }

      rc = ReadFile(fd, path, sizeof path, &cnt, 0);
      while (cnt > 0) {
	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u: ... read %u bytes from scof.\n", dev->id, cnt);
		  fflush(lib_logfile);
	    }

	    WriteFile(ch0, path, cnt, &cnt, 0);
	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u: ... wrote %u bytes to channel 0.\n", dev->id, cnt);
		  fflush(lib_logfile);
	    }

	    rc = ReadFile(fd, path, sizeof path, &cnt, 0);
      }

      CloseHandle(fd);

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u: sync channel 0...\n", dev->id);
	    fflush(lib_logfile);
      }

      rc = DeviceIoControl(ch0, UCR_SYNC, 0, 0, 0, 0, &cnt, 0);
      CloseHandle(ch0);

	/* Try to run the program. If it doesn't run right away, then
	   try again after a 20ms delay. This delay should allow the
	   ISE/SSE board to actch up with loading the firmware and
	   setting it up to go. If after a reasonable amount of time
	   it is still not ready, then give up. */
      wait_count = 100;
      do {
	      /* Run! Run! */
	    rc = DeviceIoControl(dev->isex, UCRX_RUN_PROGRAM, 0, 0, 0, 0,
				 &cnt, NULL);

	    if (rc) break;

	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u: run failed"
			  " (wait_count=%u)\n", dev->id, wait_count);
		  fflush(lib_logfile);
	    }

	    Sleep(20);
	    wait_count -= 1;
      } while (wait_count > 0);

      if (rc) {
	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u: **** ise_restart complete\n",
			  dev->id);
		  fflush(lib_logfile);
	    }

	    return ISE_OK;

      } else {

	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u: **** ise_restart failed\n",
			  dev->id);
		  fflush(lib_logfile);
	    }

	    return ISE_ERROR;
      }
}

ise_error_t ise_timeout(struct ise_handle*dev, unsigned cid, long read_timeout)
{
      BOOL rc;
      DWORD junk;
      struct ucrx_timeout_s ts;

      ts.id = cid;
      ts.read_timeout = read_timeout;
      rc = DeviceIoControl(dev->isex, UCRX_TIMEOUT, &ts, sizeof ts,
			   0, 0, &junk, 0);
      if (! rc)
	    return ISE_ERROR;

      return ISE_OK;
}

struct ise_handle* ise_bind(const char*name)
{
      struct ise_handle*dev;
      char*logpath;
      unsigned id = 0;
      int idx;

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

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise: **** ise_bind(%s)\n", name);
	    fflush(lib_logfile);
      }

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
      dev->isex = INVALID_HANDLE_VALUE;
      dev->clist = 0;

      dev->frame_isex = INVALID_HANDLE_VALUE;
      for (idx = 0 ;  idx < 16 ;  idx += 1) {
	    dev->frame[idx].base = 0;
	    dev->frame[idx].size = 0;
      }

      { char pathx[32];
        sprintf(pathx, "\\\\.\\isex%u", dev->id);

	if (lib_logfile) {
	      fprintf(lib_logfile, "ise%u: Opening control device %s\n",
		      dev->id, pathx);
	      fflush(lib_logfile);
	}

	dev->isex = CreateFile(pathx, GENERIC_READ|GENERIC_WRITE, 0, 0,
			       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	dev->frame_isex = CreateFile(pathx, GENERIC_READ|GENERIC_WRITE, 0, 0,
			       OPEN_EXISTING,
			       FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED, 0);
      }

      if (dev->isex == INVALID_HANDLE_VALUE) {

	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u: Control file failed.\n",
			  dev->id);
		  fflush(lib_logfile);
	    }

	    free(dev);
	    return 0;
      }

      return dev;
}

struct ise_handle* ise_open(const char*name)
{
      BOOL rc;
      DWORD junk;
      struct ise_handle*dev;
      char*buf = 0;
      size_t nbuf = 0;

      HANDLE fd;

      dev = ise_bind(name);
      if (dev == 0)
	    return 0;

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise: **** ise_open(%s)\n", name);
	    fflush(lib_logfile);
      }


	/* Send a reset control to the control device of the
	   board. This causes the processor on the ISE to reset into
	   its bootprom. */

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u: Resetting device\n", dev->id);
	    fflush(lib_logfile);
      }

      rc = DeviceIoControl(dev->isex, UCRX_RESET_BOARD, 0, 0, 0, 0,
			   &junk, NULL);
      if (!rc)
	    rc = DeviceIoControl(dev->isex, UCRX_RESTART_BOARD, 0, 0, 0, 0,
				 &junk, NULL);
      if (! rc) {
	    CloseHandle(dev->isex);
	    CloseHandle(dev->frame_isex);
	    free(dev);
	    return 0;
      }


	/* Now open a channel on the normal device and switch it to
	   254. This will be where I get the ident information. */

      { char path[16];
        unsigned chan;
        sprintf(path, "\\\\.\\ise%u", dev->id);

	if (lib_logfile) {
	      fprintf(lib_logfile, "ise%u: Open device %s\n", dev->id, path);
	      fflush(lib_logfile);
	}

	fd = CreateFile(path, GENERIC_READ|GENERIC_WRITE, 0, 0,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (fd == INVALID_HANDLE_VALUE) {
	      CloseHandle(dev->isex);
	      CloseHandle(dev->frame_isex);
	      free(dev);
	      return 0;
	}

	if (lib_logfile) {
	      fprintf(lib_logfile, "ise%u: switch to channel 254\n", dev->id);
	      fflush(lib_logfile);
	}

	chan = 254;
	rc = DeviceIoControl(fd, UCR_CHANNEL, &chan, sizeof chan,
			     0, 0, &junk, 0);
	if (! rc) {
	      CloseHandle(fd);
	      CloseHandle(dev->isex);
	      CloseHandle(dev->frame_isex);
	      free(dev);
	      return 0;
	}
      }

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u: reading ident data\n", dev->id);
	    fflush(lib_logfile);
      }

      WriteFile(fd,  "\n", 1, &junk, 0);

	/* Flush the write buffer to the hardware. This is not
	   strictly necessary, but might save a few nano-seconds, and
	   anyhow it can't hurt, since I know that a read follows. */
      rc = DeviceIoControl(fd, UCR_FLUSH, 0, 0, 0, 0, &junk, 0);
      if (! rc) {
	    CloseHandle(fd);
	    CloseHandle(dev->isex);
	    CloseHandle(dev->frame_isex);
	    free(dev);
	    return 0;
      }

      buf = malloc(4096);
      nbuf = 4096;

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u: (d) Waiting for monitor prompt...\n", dev->id);
	    fflush(lib_logfile);
      }

	/* Read the monitor banner message up to the first > prompt. */
      buf[0] = 0;
      while (buf[0] != '>') {
	    ReadFile(fd, buf, 1, &junk, 0);
      }

	/* Send an [i]dent command. */

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u: (d) Sending [i]dent command...\n", dev->id);
	    fflush(lib_logfile);
      }

      WriteFile(fd, "i\n", 2, &junk, 0);
      rc = DeviceIoControl(fd, UCR_FLUSH, 0, 0, 0, 0, &junk, 0);
      if (! rc) {
	    CloseHandle(fd);
	    CloseHandle(dev->isex);
	    CloseHandle(dev->frame_isex);
	    free(buf);
	    free(dev);
	    return 0;
      }


	/* Read back the ident string. Read the whole thing, up to the
	   final prompt (>) character. Make sure the allocate more
	   buffer as needed. */
      { char*cp = buf;
        for (;;) {
	      if ((cp-buf) == nbuf) {
		    buf = realloc(buf, nbuf+1024);
		    cp = buf + nbuf;
		    nbuf += 1024;
	      }

	      ReadFile(fd, cp, 1, &junk, 0);
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

	if (lib_logfile) {
	      fprintf(lib_logfile, "ise%u: (d) Got %d bytes of ident text\n", dev->id, (cp-buf));
	      fflush(lib_logfile);
	}
      }

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u: Resetting device\n", dev->id);
	    fflush(lib_logfile);
      }

      CloseHandle(fd);
      rc = DeviceIoControl(dev->isex, UCRX_RESET_BOARD, 0, 0, 0, 0,
			   &junk, NULL);

      if (!rc)
	    rc = DeviceIoControl(dev->isex, UCRX_RESTART_BOARD, 0, 0, 0, 0,
				 &junk, NULL);


      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u: ise_open(%s) complete (RESET/RESTART rc=%d).\n",
		    dev->id, name, rc);
	    fflush(lib_logfile);
      }

      return dev;
}

void ise_close(struct ise_handle*dev)
{
      unsigned idx;
      BOOL rc;
      DWORD junk;

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u: **** ise_close\n", dev->id);
	    fflush(lib_logfile);
      }

      while (dev->clist) {
	    struct ise_channel*chn = dev->clist;
	    dev->clist = chn->next;

	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u: Close channel %u\n",
			  dev->id, chn->cid);
		  fflush(lib_logfile);
	    }

	    CloseHandle(chn->fd);
	    free(chn);
      }

	/* No further use for the frames or the frame handle. This
	   deletes all the frames at the same time. */
      CloseHandle(dev->frame_isex);
      dev->frame_isex = INVALID_HANDLE_VALUE;

	/* Now clear residual data of the frame. */
      for (idx = 0 ;  idx < 16 ;  idx += 1) {
	    ise_delete_frame(dev, idx);
      }

      if (dev->version) {
	      /* If the board was opened by an ise_open, then reset
		 the board on close. */
	    if (lib_logfile) {
		  fprintf(lib_logfile, "ise%u: Reset board.\n", dev->id);
		  fflush(lib_logfile);
	    }

	    rc = DeviceIoControl(dev->isex, UCRX_RESET_BOARD,
				 0, 0, 0, 0, &junk, NULL);
	    if (!rc)
		  rc = DeviceIoControl(dev->isex, UCRX_RESTART_BOARD,
				       0, 0, 0, 0, &junk, NULL);

	    CloseHandle(dev->isex);

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

      if (lib_logfile) {
	    fprintf(lib_logfile, "ise%u: **** ise_close complete\n", dev->id);
	    fflush(lib_logfile);
      }

      free(dev);
}
