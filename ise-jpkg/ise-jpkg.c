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
# include  <stdio.h>
# include  <stdlib.h>
# include  <malloc.h>
# include  <assert.h>

# define DEFAULT_ISE "ise0"

static void status_msgs(const char*txt)
{
      printf("Status: %s\n", txt);
}

int main(int argc, char*argv[])
{
      int idx;
      int rc;
      ise_error_t ise_rc;
      const char*ise = DEFAULT_ISE;
      struct ise_handle*dev = 0;

      for (idx = 1 ; idx < argc ; idx += 1) {

	    if (argv[idx][0] != '-')
		  break;

	    switch (argv[idx][1]) {

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

      dev = ise_open(ise);
      if (dev == 0) {
	    fprintf(stderr, "Unable to open ISE/JSE device %s\n", ise);
	    return 1;
      }

      unsigned char*file_buf = 0;
      size_t nbuf = 0;
      while (idx < argc) {
	    size_t use_size;
	    size_t count;
	    FILE*fd = fopen(argv[idx], "rb");
	    if (fd == 0) {
		  fprintf(stderr, "Unable to open %s\n", argv[idx]);
		  idx += 1;
		  continue;
	    }

	    rc = fseek(fd, 0, SEEK_END);
	    assert(rc >= 0);
	    use_size = ftell(fd);

	    rc = fseek(fd, 0, SEEK_SET);
	    assert(rc >= 0);

	    if (use_size > nbuf) {
		  if (file_buf)
			free(file_buf);
		  file_buf = malloc(use_size);
		  nbuf = use_size;
	    }

	    count = fread(file_buf, 1, use_size, fd);
	    assert(count == use_size);

	    fclose(fd);

	    ise_rc = ise_send_package(dev, argv[idx], file_buf, use_size, status_msgs);
	    assert(ise_rc == ISE_OK);

	    idx += 1;
      }

      ise_close(dev);
      return 0;
}
