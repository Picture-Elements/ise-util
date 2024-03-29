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


struct frame_data __libiseio_plug_frames [16];


const struct ise_plug_frame*ise_plug_frame_lock(int frm)
{
      if (frm < 0)
	    return 0;
      if (frm >= 16)
	    return 0;

      struct frame_data*fdp = __libiseio_plug_frames+frm;
      pthread_mutex_lock(&fdp->sync);
      return &fdp->base_data;
}


void ise_plug_frame_unlock(const struct ise_plug_frame*framep)
{
      struct frame_data*fdp = __libiseio_plug_frames+framep->id;
      pthread_mutex_unlock(&fdp->sync);
}
