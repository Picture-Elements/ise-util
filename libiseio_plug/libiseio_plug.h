#ifndef __libiseio_plug_H
#define __libiseio_plug_H
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

# include  <stddef.h>

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN extern
#endif

/*
 * This function should be called by your main() program. It does not
 * return (except to end the process) and takes over the main thread
 * and command line arguments. Your application will be given its own
 * thread later.
 */
EXTERN int ise_plug_main(int argc, char*argv[]);

/*
 * The ise_plug_readln function reads a line from the given
 * channel. If the line is not yet complete, then the thread blocks
 * until a line is completed (or the timeout runs out.) The return
 * code is the size of the line (which may be greater then nbuf!) or 0
 * if the read times out.
 *
 * If udelay==0, the thread will not block. It will look for a line,
 * and if none is present, return 0.
 *
 * If udelay<0, the ise_plug_readln will not timeout.
 *
 * If udelay>0, the ise_plug_readln will timeout in that many uSeconds
 * if no line arrives.
 */
EXTERN int ise_plug_readln(int chn, char*buf, size_t nbuf, long udelay);

/*
 * The ise_plug_writeln function writes a like to the given
 * channel. The argument is a string, and the functio adds the
 * terminating \n character. If the channel is not connected, the data
 * is quietly dropped.
 */
EXTERN int ise_plug_writeln(int chn, const char*data);


/*
 * The frames are shared memory regions, shared between the plugin and
 * the host application. Use the ise_plug_frame_lock function to grab
 * a reference to the frame. The library will return a pointer to an
 * ise_plug_frame that is filled in with the information about the
 * frame. When done with the frame, use ise_plug_frame_unlock to
 * release the frame.
 *
 * It is cheap to lock/unlock, and potentially dangerous to hold it
 * too long, so grab the lock when you need it, and release it as soon
 * as you are done.
 *
 * It is up to the host to create/destroy the frames. The frame will
 * not change while locked, so the plugin can rely on the frame data
 * being stable while it holds the lock. That's the reason to hold the
 * lock.
 *
 * If the frame is not present, then the base pointer will be nil. The
 * user still must unlock the frame. Otherwise, the host will be
 * locked out of creating the frame later.
 */
struct ise_plug_frame {
      int id;
      void*base;
      size_t size;
};

EXTERN const struct ise_plug_frame* ise_plug_frame_lock(int frm);
EXTERN void ise_plug_frame_unlock(const struct ise_plug_frame*info);

/* **** **** */

/* **
 * The following functions are not provided by the library, but are
 * referenced by the library. The application provides these functions
 * to give the plugin its behavior.
 */

/*
 * This is the "main" function for the application. This is put in a
 * thread of its own. When the host starts the application, the
 * library creates a thread that calls this application. The
 * data/ndata is a pointer to a data buffer that contains startup
 * information. It is up to the application to interpret it.
 */
EXTERN void ise_plug_application(void*data, size_t ndata);

/* **** **** */

/*
 * What all this means...
 *
 * The libiseio_plug provides an alternative target for the libiseio.h
 * API that applications use. The application that uses libiseio.h
 * sees the image processing engine as a remote device that has a
 * collection of communications channels for command/control, and some
 * shared memory for bulk data transfer.
 *
 * The application opens a device with the ise_open function. When the
 * device is an SSE/JSE device, the function call is something like
 * this:
 *
 *   struct ise_handle*dev = ise_open("ise0");
 *
 * If the device name is of the form "ise<N>" then an ISE/SSE/JSE
 * board is opened, and the libiseio_plug does not factor.
 *
 * But if the device name is of the form "plug:<name>" then the
 * libiseio library will name the <name> to be the name of an
 * executable in a predefined location. This program is built using
 * this libiseio_plug.h API.
 *
 * - struct ise_handle*dev = ise_open(("plug:<name>");
 * The ise_open("plug:<name>") function loads the plugin by executing
 * its binary and connecting to command channels. If the plugin
 * program is not found or won't execute, then the ise_open function
 * returns a nil handle.
 *
 * The ise_open checks that the plug-in starts by sending an OPEN
 * command through a command channel that is created as part of the
 * ise_open process. The libiseio_plug library handles this OPEN
 * command internally, sets up internal state, and acknowledges the
 * command. After the acknowledgement, the ise_open returns with the
 * dev handle.
 *
 * - struct ise_handle*dev = ise_bind("plug:<name>");
 * The ise_bind function does not apply to plugins. Do not use it.
 *
 * - const char*txt = ise_prom_version(dev);
 * ???
 *
 * - ise_restart(dev, "firm");
 * This sends a start command to the plug-in process, which causes it
 * to create a thread to call the ise_plug_application. The function
 * is provided by the plug-in writer to give the plug-in its behavior.
 * This does NOT download an actual firmware when talking with a
 * plug-in.
 *
 * - ise_writeln
 * - ise_readln
 * The application uses these functions to send commands to and
 * receive responses from the ISE/SSE/JSE firmware. In the plug-in
 * context, the ise_writeln function provides data that the plug-in
 * can read with the ise_plug_readln() function, and vis versa with
 * ise_plug_writeln an ise_readln.
 *
 * - ise_make_frame / ise_delete_frame
 * The application uses these functions to manage shared memory
 * buffers. These cause commands to be sent to the plug-in, where the
 * libiseio_plug library handles the frame binding transparently. The
 * plug-in code accesses these frames using the ise_plug_frame_lock
 * function.
 *
 * - ise_close
 * This closes all the channels to the plug_in and invokes a
 * restart. (NOTE: Not implemented yet.)
 */

#endif
