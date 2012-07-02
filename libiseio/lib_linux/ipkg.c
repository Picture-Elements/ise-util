/*
 * Copyright (c) 2009 Picture Elements, Inc.
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

# include  <stdlib.h>
# include  <unistd.h>
# include  <stdio.h>
# include  <stdint.h>
# include  <string.h>
# include  <errno.h>
# include  <fcntl.h>

# include  <assert.h>


#define IH_MAGIC	0x27051956	/* Image Magic Number		*/
#define IH_NMLEN		32	/* Image Name Length		*/
#define IH_CPU_PPC		7	/* PowerPC	*/
#define IH_TYPE_FILESYSTEM	7	/* Filesystem Image (any type)	*/

typedef struct image_header {
	uint32_t	ih_magic;	/* Image Header Magic Number	*/
	uint32_t	ih_hcrc;	/* Image Header CRC Checksum	*/
	uint32_t	ih_time;	/* Image Creation Timestamp	*/
	uint32_t	ih_size;	/* Image Data Size		*/
	uint32_t	ih_load;	/* Data	 Load  Address		*/
	uint32_t	ih_ep;		/* Entry Point Address		*/
	uint32_t	ih_dcrc;	/* Image Data CRC Checksum	*/
	uint8_t		ih_os;		/* Operating System		*/
	uint8_t		ih_arch;	/* CPU architecture		*/
	uint8_t		ih_type;	/* Image Type			*/
	uint8_t		ih_comp;	/* Compression Type		*/
	uint8_t		ih_name[IH_NMLEN];	/* Image Name		*/
} image_header_t;


uint32_t u32_to_be(uint32_t val)
{
      unsigned long res = 0;

      res |= (val >> 24) & 0x000000ff;
      res |= (val >>  8) & 0x0000ff00;
      res |= (val <<  8) & 0x00ff0000;
      res |= (val << 24) & 0xff000000;
      return res;
}

ise_error_t ise_send_package(struct ise_handle*dev, const char*pkg_name,
			     const void*pkg_data, size_t ndata,
			     void (*status_msgs)(const char*txt))
{
      struct image_header head;
      struct ise_channel*ch0 = __ise_find_channel(dev, 0);
      struct ise_channel*ch1 = __ise_find_channel(dev, 1);
      char buf[1024];

	/* This does not work with device opened with ise_bind. */
      if (dev->version == 0)
	    return ISE_ERROR;
      if (ch0)
	    return ISE_CHANNEL_BUSY;
      if (ch1)
	    return ISE_CHANNEL_BUSY;

      ise_channel(dev, 1);
      ch1 = __ise_find_channel(dev, 1);
      if (ch1 == 0)
	    return ISE_ERROR;

      ise_channel(dev, 0);
      ch0 = __ise_find_channel(dev, 0);
      if (ch0 == 0)
	    return ISE_ERROR;

      head.ih_magic = u32_to_be(IH_MAGIC);
      head.ih_hcrc  = 0;
      head.ih_time = 0;
      head.ih_size = u32_to_be(ndata);
      head.ih_load = 0;
      head.ih_ep = 0;
      head.ih_dcrc = 0;
      head.ih_os = 0;
      head.ih_arch = IH_CPU_PPC;
      head.ih_type = IH_TYPE_FILESYSTEM;
      head.ih_comp = 0;
      strncpy(head.ih_name, pkg_name, IH_NMLEN);

      dev->fun->write(dev, ch0, &head, sizeof head);
      dev->fun->write(dev, ch0, pkg_data, ndata);

	/* Force a SYNC to get all the data downloaded to the board
	   and ready for a restart. */
      dev->fun->channel_sync(dev, ch0);

	/* Run the program. This will probably time out, but that's OK
	   becuase we will use the readback to synchronize with the
	   output. */
      dev->fun->run_program(dev);

      ise_readln(dev, 1, buf, sizeof buf);
      while (strcmp(buf, "*done*") != 0) {
	    if (__ise_logfile)
		  fprintf(__ise_logfile, "%s: package: %s\n", dev->id_str, buf);
	    if (status_msgs)
		  status_msgs(buf);
	    ise_readln(dev, 1, buf, sizeof buf);
      }

      if (status_msgs)
	    status_msgs(buf);

      assert(dev->clist == ch0);
      dev->clist = ch0->next;
      free(ch0);

      assert(dev->clist == ch1);
      dev->clist = ch1->next;
      free(ch1);

      return ISE_OK;
}
