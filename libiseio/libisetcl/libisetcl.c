/*
 * Copyright (c) 2001 Picture Elements, Inc.
 *    Stephen Williams (steve@picturel.com)
 */
#if !defined(WINNT)
#ident "$Id: libisetcl.c,v 1.2 2002/07/23 21:41:18 steve Exp $"
#endif

# include  <tcl.h>
# include  <libiseio.h>
# include  <malloc.h>
# include  <stdlib.h>
# include  <assert.h>

/*
 * This file implements the TCL interface to the libiseio API. For the
 * most part, this is simply a thin veneer over the C API, since the C
 * API is pretty abstract anyhow.
 *
 * ise_open <device> <name>
 *    This command opens the specified device, and creates an object
 *    with the given <name>. This creates a new command <name> that
 *    then supports the functions on the handle.
 *
 * <name> restart <firmware>
 *    This method loads and starts the specified firmware on the
 *    target ISE board.
 *
 * <name> channel <channel>
 *    This method opens the specified channel on the target ISE
 *    board. This must be done before readln or writeln on the given
 *    channel will work.
 *
 * <name> writeln <channel> <text>
 *    This method writes a line of text to the channel. The text is
 *    passed as a single argument.
 *
 * <name> readln <channel>
 *    This method reads from the channel a line of text. The method
 *    returns as a value the line that was read. The newline is
 *    stripped off the string.
 *
 *
 * <name> make-frame <id> <size>
 *    This method creates a frame of the given id and the requested
 *    size. The method returns as a value the actual size of the
 *    created frame. If the frame already exists, then the existing
 *    frame size is returned.
 *
 * <name> delete-frame <id>
 *    This method releases a frame, if it exists.
 *
 * <name> write-frame <id> <file> <off> <size>
 *    This method writes the contents of a frame into an opened
 *    file. The file is the name of an existing file by the "open"
 *    command.
 *
 * <name> read-frame <id> <file> <off> <size>
 *    This method writes the contents of an opened file into a
 *    frame. The file is the name of an existing file by the "open"
 *    command. This is the opposite of write-frame.
 *
 * Removeing <name> by a command like "rename foo {}" causes the
 * device to be closed and released.
 */

struct isetcl_handle {
      struct ise_handle*dev;
      void*  frame_buf[16];
      size_t frame_siz[16];
};

static int object_cmd(ClientData cd, Tcl_Interp*interp,
		      int argc, char*argv[])
{
      struct isetcl_handle *fd = (struct isetcl_handle*)cd;

      if (argc < 2) {
	    Tcl_AppendResult(interp, argv[0], ": missing subcommand", 0);
	    return TCL_ERROR;
      }

      if (strcmp(argv[1],"channel") == 0) {
	    unsigned channel;
	    ise_error_t rc;

	    if (argc != 3) {
		  Tcl_AppendResult(interp, argv[0], ": channel usage: ",
				   "channel <channel>", 0);
		  return TCL_ERROR;
	    }

	    channel = strtoul(argv[2], 0, 0);
	    rc = ise_channel(fd->dev, channel);
	    if (rc == ISE_OK)
		  return TCL_OK;

	    Tcl_AppendResult(interp, argv[0], ": channel error: ",
			     ise_error_msg(rc), 0);
	    return TCL_ERROR;
      }

      if (strcmp(argv[1],"readln") == 0) {
	    unsigned channel;
	    char text[8*1024];
	    ise_error_t rc;

	    if (argc != 3) {
		  Tcl_AppendResult(interp, argv[0], ": readln usage: ",
				   "readln <channel>", 0);
		  return TCL_ERROR;
	    }

	    channel = strtoul(argv[2], 0, 0);
	    rc = ise_readln(fd->dev, channel, text, sizeof text);
	    if (rc == ISE_OK) {
		  Tcl_SetResult(interp, text, TCL_VOLATILE);
		  return TCL_OK;
	    }

	    Tcl_AppendResult(interp, argv[0], ": readln error: ",
			     ise_error_msg(rc), 0);
	    return TCL_ERROR;
      }

      if (strcmp(argv[1],"restart") == 0) {
	    ise_error_t rc;

	    if (argc != 3) {
		  Tcl_AppendResult(interp, argv[0], ": restart usage: ",
				   "restart <firmware name>", 0);
		  return TCL_ERROR;
	    }

	    rc = ise_restart(fd->dev, argv[2]);
	    if (rc == ISE_OK)
		  return TCL_OK;

	    Tcl_AppendResult(interp, argv[0], ": restart error: ",
			     ise_error_msg(rc), 0);
	    return TCL_ERROR;
      }


      if (strcmp(argv[1],"writeln") == 0) {
	    unsigned channel;
	    ise_error_t rc;

	    if (argc != 4) {
		  Tcl_AppendResult(interp, argv[0], ": writeln usage: ",
				   "writeln <channel> <text>", 0);
		  return TCL_ERROR;
	    }

	    channel = strtoul(argv[2], 0, 0);
	    rc = ise_writeln(fd->dev, channel, argv[3]);
	    if (rc == ISE_OK)
		  return TCL_OK;

	    Tcl_AppendResult(interp, argv[0], ": writeln error: ",
			     ise_error_msg(rc), 0);
	    return TCL_ERROR;
      }

      if (strcmp(argv[1],"make-frame") == 0) {
	    unsigned id;
	    size_t size;

	    if (argc != 4) {
		  Tcl_AppendResult(interp, argv[0], ": make-frame usage: ",
				   "make-frame <id> <size>", 0);
		  return TCL_ERROR;
	    }

	    id = strtoul(argv[2],0,0);
	    size = strtoul(argv[3],0,0);

	    if (id >= 16) {
		  Tcl_SetResult(interp, "frame id must be 0-15", TCL_STATIC);
		  return TCL_ERROR;
	    }

	    if (size == 0) {
		  Tcl_SetResult(interp, "frame size must be >0", TCL_STATIC);
		  return TCL_ERROR;
	    }

	    if (fd->frame_siz[id] > 0) {
		  char text[64];
		  sprintf(text, "%lu", fd->frame_siz[id]);
		  Tcl_SetResult(interp, text, TCL_VOLATILE);
		  return TCL_OK;
	    }

	    fd->frame_siz[id] = size;
	    fd->frame_buf[id] = ise_make_frame(fd->dev, id,
					       &fd->frame_siz[id]);
	    if (fd->frame_buf[id] != 0) {
		  char text[64];
		  sprintf(text, "%lu", fd->frame_siz[id]);
		  Tcl_SetResult(interp, text, TCL_VOLATILE);
		  return TCL_OK;
	    }

	    fd->frame_siz[id] = 0;
	    Tcl_SetResult(interp, "error makeing frame", TCL_STATIC);
	    return TCL_ERROR;
      }

      if (strcmp(argv[1],"delete-frame") == 0) {
	    unsigned id;

	    if (argc != 3) {
		  Tcl_AppendResult(interp, argv[0], ": delete-frame usage: ",
				   "delete-frame <id>", 0);
		  return TCL_ERROR;
	    }

	    id = strtoul(argv[2],0,0);

	    if (id >= 16) {
		  Tcl_SetResult(interp, "frame id must be 0-15", TCL_STATIC);
		  return TCL_ERROR;
	    }

	    if (fd->frame_siz[id] > 0) {
		  ise_delete_frame(fd->dev, id);
		  fd->frame_siz[id] = 0;
		  fd->frame_buf[id] = 0;
	    }

	    return TCL_OK;
      }

      if (strcmp(argv[1],"write-frame") == 0) {
	    unsigned id;
	    unsigned long off;
	    unsigned long cnt;
	    Tcl_Channel file;

	    int rc;
	    const char*bufp;

	    if (argc != 6) {
		  Tcl_AppendResult(interp, argv[0], ": write-frame usage: ",
				   "write-frame <id> <file> <off> <size>", 0);
		  return TCL_ERROR;
	    }

	    id = strtoul(argv[2],0,0);

	    if (id >= 16) {
		  Tcl_SetResult(interp, "frame id must be 0-15", TCL_STATIC);
		  return TCL_ERROR;
	    }

	    file = Tcl_GetChannel(interp, argv[3], 0);
	    if (file == 0) {
		  Tcl_AppendResult(interp, argv[0], ": no such channel: ",
				   argv[3], 0);
		  return TCL_ERROR;
	    }

	    off = strtoul(argv[4],0,0);
	    cnt = strtoul(argv[5],0,0);

	    if (off >= fd->frame_siz[id])
		  return TCL_OK;

	    if ((off + cnt) > fd->frame_siz[id])
		  cnt = fd->frame_siz[id] - off;

	    bufp = (const char*)fd->frame_buf[id];
	    bufp += off;
	    rc = Tcl_Write(file, bufp, cnt);
	    if (rc < 0) {
		  Tcl_SetResult(interp, "error writing to file", TCL_STATIC);
		  return TCL_ERROR;
	    }

	    return TCL_OK;
      }

      if (strcmp(argv[1],"read-frame") == 0) {
	    unsigned id;
	    unsigned long off;
	    unsigned long cnt;
	    Tcl_Channel file;

	    int rc;
	    char*bufp;

	    if (argc != 6) {
		  Tcl_AppendResult(interp, argv[0], ": read-frame usage: ",
				   "read-frame <id> <file> <off> <size>", 0);
		  return TCL_ERROR;
	    }

	    id = strtoul(argv[2],0,0);

	    if (id >= 16) {
		  Tcl_SetResult(interp, "frame id must be 0-15", TCL_STATIC);
		  return TCL_ERROR;
	    }

	    file = Tcl_GetChannel(interp, argv[3], 0);
	    if (file == 0) {
		  Tcl_AppendResult(interp, argv[0], ": no such channel: ",
				   argv[3], 0);
		  return TCL_ERROR;
	    }

	    off = strtoul(argv[4],0,0);
	    cnt = strtoul(argv[5],0,0);

	    if (off >= fd->frame_siz[id])
		  return TCL_OK;

	    if ((off + cnt) > fd->frame_siz[id])
		  cnt = fd->frame_siz[id] - off;

	    bufp = (char*)fd->frame_buf[id];
	    bufp += off;
	    rc = Tcl_Read(file, bufp, cnt);
	    if (rc < 0) {
		  Tcl_SetResult(interp, "error reading from file", TCL_STATIC);
		  return TCL_ERROR;
	    }

	    return TCL_OK;
      }

      Tcl_AppendResult(interp, argv[0], ": invalid subcommand: ",
		       argv[1], 0);
      return TCL_ERROR;
}

static void object_delete(ClientData cd)
{
      struct isetcl_handle *fd = (struct isetcl_handle*)cd;
      ise_close(fd->dev);
      free(fd);
}

/*
 * This function implements the ise_open command. This creates a
 * handle for refering to the device, and opens the device itself.
 */
static int ise_open_cmd(ClientData cd, Tcl_Interp*interp,
			int argc, char*argv[])
{
      struct isetcl_handle *fd;

      if (argc != 3) {
	    Tcl_SetResult(interp, "wrong # args: "
			  "ise_open <device> <name>", TCL_STATIC);
	    return TCL_ERROR;
      }

      fd = calloc(1, sizeof(struct isetcl_handle));
      fd->dev = ise_open(argv[1]);
      if (fd->dev == 0) {
	    free(fd);
	    Tcl_AppendResult(interp, "error opening ", argv[1], 0);
	    return TCL_ERROR;
      }

      Tcl_CreateCommand(interp, argv[2], &object_cmd, fd, &object_delete);
      return TCL_OK;
}

int Isetcl_Init(Tcl_Interp*interp)
{
      Tcl_CreateCommand(interp, "ise_open", ise_open_cmd, 0, 0);
      return TCL_OK;
}

/*
 * $Log: libisetcl.c,v $
 * Revision 1.2  2002/07/23 21:41:18  steve
 *  Add the read-frame command.
 *
 * Revision 1.1  2002/02/10 05:29:59  steve
 *  Add tcl interface to libiseio.
 *
 */

