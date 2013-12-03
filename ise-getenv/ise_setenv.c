/*
 * Copyright (c) 2000-2004 Picture Elements, Inc.
 *    Stephen Williams (steve@picturel.com)
 */
#if !defined(WINNT)
#ident "$Id: ise_setenv.c,v 1.4 2004/04/13 02:43:09 steve Exp $"
#endif

# include  <libiseio.h>
# include  <stdio.h>
# include  <stdlib.h>
# include  <string.h>

static char* parms[256];
static char* vals [256];
static unsigned nparms = 0;

main(int argc, char*argv[])
{
      int idx;
      ise_error_t ise_rc;

      const char*name;
      const char*path;
      const char*firmware;
      unsigned bank = 0;

      struct ise_handle*dev;
      FILE*file;

      char lbuf[256];

      if (argc < 3) {
	    fprintf(stderr, "Usage: ise_setenv <device> <env_file> [<bank>]\n");
	    return -1;
      }

      name = argv[1];
      path = argv[2];

      if (argc >= 4) {
	    bank = strtoul(argv[3],0,0);
	    if (bank >= 8) {
		  fprintf(stderr, "Unvalid bank.\n");
		  return -1;
	    }
      }

      file = fopen(path, "r");
      if (file == 0) {
	    fprintf(stderr, "Unable to open %s\n", path);
	    return -1;
      }

      nparms = 0;
      while (fgets(lbuf, sizeof lbuf, file)) {
	    char*cp = strchr(lbuf, '\n');
	    if (cp) *cp = 0;

	    if (strlen(lbuf) == strspn(lbuf, " \t"))
		  continue;

	    cp = strchr(lbuf, '=');
	    if (cp == 0) {
		  fprintf(stderr, "Invalid parameter line: %s\n", lbuf);
		  return -1;
	    }

	    *cp = 0;
	    parms[nparms] = strdup(lbuf);
	    cp += 1;
	    vals[nparms] = strdup(cp);

	    nparms += 1;
	    if (nparms == 256)
		  break;
      }

      dev = ise_open(name);
      if (dev == 0) {
	    fprintf(stderr, "%s: unable to open ISE board.\n", name);
	    return -1;
      }

	/* Figure out the basic board type from the flash revision
	   information, and select an environ firmware to use. */
      if (strncmp(ise_prom_version(dev), "SSE", 3) == 0)
	    firmware = "environ-i960_sse";
      else if (strncmp(ise_prom_version(dev), "JSE", 3) == 0)
	    firmware = "environ-jse";
      else if (strncmp(ise_prom_version(dev), "ZJSE", 4) == 0)
	    firmware = "environ-zjse";
      else
	    firmware = "environ-i960_ise";


      ise_rc = ise_restart(dev, firmware);
      if (ise_rc != ISE_OK) {
	    fprintf(stderr, "%s: %s\n", name, ise_error_msg(ise_rc));
	    return -1;
      }

      ise_rc = ise_channel(dev, 3);
      if (ise_rc != ISE_OK) {
	    fprintf(stderr, "%s: error opening channel 3: %s\n",
		    name, ise_error_msg(ise_rc));
	    return -1;
      }


      ise_rc = ise_writeln(dev, 3, "reset");

      for (idx = 0 ;  idx < nparms ;  idx += 1) {
	    sprintf(lbuf, "set env %s %s", parms[idx], vals[idx]);
	    ise_rc = ise_writeln(dev, 3, lbuf);
	    ise_rc = ise_readln(dev, 3, lbuf, sizeof lbuf);
	    if (strcmp(lbuf, "OK") != 0) {
		  fprintf(stderr, "%s: Strange response from ISE? %s\n",
			  name, lbuf);
		  return -1;
	    }
      }

      sprintf(lbuf, "write %u", bank);
      ise_rc = ise_writeln(dev, 3, lbuf);
      lbuf[0] = 0;
      ise_rc = ise_readln(dev, 3, lbuf, sizeof lbuf);
      if (ise_rc != ISE_OK) {
	    fprintf(stderr, "%s: error reading channel 3: %s\n",
		    name, ise_error_msg(ise_rc));
	    return -1;
      }

      while (strcmp(lbuf, "OK") != 0) {

	    printf("%s\n", lbuf);

	    ise_rc = ise_readln(dev, 3, lbuf, sizeof lbuf);
	    if (ise_rc != ISE_OK) {
		  fprintf(stderr, "%s: error reading channel 3: %s\n",
			  name, ise_error_msg(ise_rc));
		  return -1;
	    }
      }

      printf("OK\n");
      fflush(stdout);
      ise_close(dev);

      return 0;
}

/*
 * $Log: ise_setenv.c,v $
 * Revision 1.4  2004/04/13 02:43:09  steve
 *  Detect JSE and select JSE firmware.
 *
 * Revision 1.3  2003/12/12 23:03:00  steve
 *  Support for multiple banks of variables.
 *
 * Revision 1.2  2001/10/12 20:52:30  steve
 *  Select firmware for board type.
 *
 * Revision 1.1  2000/07/19 18:48:05  steve
 *  environ util installed.
 *
 */

