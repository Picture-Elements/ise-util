#ifndef __priv_H
#define __priv_H
/*
 * Copyright (c) 2009,2012 Picture Elements, Inc.
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
# include  <stdio.h>

struct ise_channel {
      struct ise_channel*next;
      unsigned cid;
      int fd;
      char buf[4096];
      unsigned ptr, fill;
};

struct ise_handle {
      char*id_str;
      int isex;
      char*version;
      struct ise_channel*clist;

      struct {
	    void*base;
	    size_t size;
      } frame[16];

	/* Low level implementation functions. */
      const struct ise_driver_functions*fun;
};

extern FILE*__ise_logfile;
extern struct ise_channel*__ise_find_channel(struct ise_handle*dev, unsigned cid);


struct ise_driver_functions {

	/* Check if the id string passed to the device suites this
	   driver. If not, then return 0. Otherwise, return !0. This
	   is only called once during ise_bind. */
      int(*probe_id)(struct ise_handle*dev);

	/* Connect to the control channel for the bound board. */
      ise_error_t (*connect)(struct ise_handle*dev);

	/* Restart the board, or put it in a state where it is able to
	   receive firmware and related commands. */
      ise_error_t (*restart)(struct ise_handle*dev);

	/* Send a command to run the program on the remote. */
      ise_error_t (*run_program)(struct ise_handle*dev);

	/* Open a channel by number and return the raw fd. */
      ise_error_t (*channel_open)(struct ise_handle*dev,
				  struct ise_channel*chn);
      ise_error_t (*channel_sync) (struct ise_handle*dev,
				   struct ise_channel*chn);
      ise_error_t (*channel_close)(struct ise_handle*dev,
				   struct ise_channel*chn);

	/* Set the read timeout for a channel */
      ise_error_t (*timeout)(struct ise_handle*dev, unsigned cid,
			     long read_timeout);

      ise_error_t (*make_frame)(struct ise_handle*dev, unsigned id);
      void        (*delete_frame)(struct ise_handle*dev, unsigned id);

	/* Write raw data through the channel */
      ise_error_t (*write)(struct ise_handle*dev,
			   struct ise_channel*chn,
			   const void*buf, size_t nbuf);

	/* Write a line of text to the channel. */
      ise_error_t (*writeln)(struct ise_handle*dev,
			     struct ise_channel*chn,
			     const char*text);

	/* Read bytes of text into the channel buffer. */
      ise_error_t (*readbuf)(struct ise_handle*dev, struct ise_channel*chn);
};

extern const struct ise_driver_functions __driver_ise;
extern const struct ise_driver_functions __driver_plug;

#endif
