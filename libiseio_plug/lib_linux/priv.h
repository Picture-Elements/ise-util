#ifndef __priv_H
#define __priv_H
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
# include  <stddef.h>
# include  <stdio.h>
# include  <pthread.h>

extern FILE*__libiseio_plug_log;

/*
 * This is the control channel to the host.
 */
extern int __libiseio_plug_isex;

/*
 * Information about open channels.
 */
struct channel_data {
      int fd;
      char buf[8*1024];
      size_t buf_fil;

      pthread_mutex_t sync;
      pthread_cond_t data_arrival;
};

extern struct channel_data __libiseio_channels [256];

/*
 * Information about open frames
 */
struct frame_data {
      struct ise_plug_frame base_data;

      pthread_mutex_t sync;
};

extern struct frame_data __libiseio_plug_frames [16];

extern void __libiseio_process_command(char*line);


#endif
