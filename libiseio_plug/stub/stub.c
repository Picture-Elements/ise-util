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

/*
 * This is a trivial example program that demonstrates the basics of
 * the libiseio_plug library.
 */
# include  "libiseio_plug.h"
# include  <stdio.h>
# include  <string.h>

/*
 * main is trivial. Keep it simple. It exists solely to call the
 * libiseio_plug_main() function. This is all that exists of the main
 * thread. The body of your application will be called elsewhere, in
 * another thread.
 */
int main(int argc, char*argv[])
{
      return ise_plug_main(argc, argv);
}

/*
 * This is the body of the application. This is created in a different
 * thread (pthread) from the main above, but it is the only thread
 * that you the application writer obviously see, unless you create
 * threads of your own.
 */
void ise_plug_application(void*data, size_t ndata)
{
      char buf[16*1024];

      fprintf(stdout, "START STUB\n");
      fflush(stdout);

	/* The trivial task for this stub is to return the message
	   that was sent on channel 6. */
      for (;;) {
	    int rc = ise_plug_readln(6, buf, sizeof buf, -1);
	    if (rc <= 0) continue;

	    fprintf(stdout, "GOT: %s\n", buf);
	    fflush(stdout);

	      /* For demonstration, grab frame 0, and lets see what
		 the frame bytes look like. Also write a few bytes for
		 the host. */
	    const struct ise_plug_frame*fr0 = ise_plug_frame_lock(0);
	    if (fr0 && fr0->base) {
		  fprintf(stdout, "FRAME[0].size = %zu\n", fr0->size);
		  if (fr0->size >= 4) {
			unsigned char*ptr = (unsigned char*) fr0->base;
			fprintf(stdout, "FRAME[0] = 0x%02x 0x%02x 0x%02x 0x%02x...\n",
				ptr[0], ptr[1], ptr[2], ptr[3]);
			strncat(ptr, buf, fr0->size);
		  }
	    }

	      /* Always unlock the frame, even if it proved to be
		 empty. Skip the unlock only if the lock error'ed
		 out. We want to do this before doing any blocking
		 operations so that we don't tie up (or possibly
		 deadlock) the host. */
	    if (fr0) ise_plug_frame_unlock(fr0);

	    ise_plug_writeln(6, buf);
      }
}
