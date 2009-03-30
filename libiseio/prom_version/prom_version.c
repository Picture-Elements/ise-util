/*
 * Copyright (c) 2000 Picture Elements, Inc.
 *    Stephen Williams (steve@picturel.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version. In order to redistribute the software in
 *    binary form, you will need a Picture Elements Binary Software
 *    License.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 *  ---
 *    You should also have recieved a copy of the Picture Elements
 *    Binary Software License offer along with the source. This offer
 *    allows you to obtain the right to redistribute the software in
 *    binary (compiled) form. If you have not received it, contact
 *    Picture Elements, Inc., 777 Panoramic Way, Berkeley, CA 94704.
 */
#if !defined(WINNT)
#ident "$Id: prom_version.c,v 1.1 2000/06/29 18:45:38 steve Exp $"
#endif

# include  <libiseio.h>
# include  <stdio.h>

int main(int argc, char*argv[])
{
      int idx;

      for (idx = 1 ;  idx < argc ;  idx += 1) {
	    struct ise_handle*dev = ise_open(argv[idx]);
	    if (dev == 0) {
		  fprintf(stderr, "%s: Unable to open ISE device.\n",
			  argv[idx]);
		  continue;
	    }

	    printf("BASEBOARD FLASH version data for %s\n", argv[idx]);
	    puts(ise_prom_version(dev));
	    ise_close(dev);
      }

      return 0;
}

/*
 * $Log: prom_version.c,v $
 * Revision 1.1  2000/06/29 18:45:38  steve
 *  Add the prom_version command.
 *
 */

