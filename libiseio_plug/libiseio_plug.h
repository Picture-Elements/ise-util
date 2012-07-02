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

#endif
