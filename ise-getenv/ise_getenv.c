/*
 * Copyright (c) 2000-2004 Picture Elements, Inc.
 *    Stephen Williams (steve@picturel.com)
 */
#if !defined(WINNT)
#ident "$Id: ise_getenv.c,v 1.5 2006/01/24 00:48:54 steve Exp $"
#endif

# include  <libiseio.h>
# include  <stdio.h>
# include  <string.h>

static int raw_dump(struct ise_handle*dev, const char*name, unsigned bank);
static int variable_read(struct ise_handle*dev, const char*name, unsigned bank);

main(int argc, char*argv[])
{
      struct ise_handle*dev;
      ise_error_t ise_rc;
      const char*name;
      const char*firmware;
      int raw_flag = 0;
      unsigned bank = 0;
      int rc;

      if (argc < 2) {
	    fprintf(stderr, "Usage: ise_getenv <device> [<bank>]\n");
	    fprintf(stderr, "       <device> is ise0-iseN\n");
	    fprintf(stderr, "       <bank> is 0-8 or r0-r8\n");
	    return -1;
      }

      name = argv[1];

      if (argc >= 3) {
	    if (argv[2][0] == 'r') {
		  raw_flag = 1;
		  bank = strtoul(argv[2]+1,0,0);
	    } else {
		  bank = strtoul(argv[2],0,0);
	    }
	    if (bank >= 8) {
		  fprintf(stderr, "%s: invalid bank %u. (0-7 are valid)\n",
			  argv[0], argv[2]);
		  return -1;
	    }
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

      if (raw_flag)
	    rc = raw_dump(dev, name, bank);
      else
	    rc = variable_read(dev, name, bank);

      ise_close(dev);

      return rc;
}

static int variable_read(struct ise_handle*dev, const char*name, unsigned bank)
{
      ise_error_t ise_rc;
      char buf[128];

      sprintf(buf, "<read>read %u", bank);
      ise_rc = ise_writeln(dev, 3, buf);
      buf[0] = 0;
      ise_rc = ise_readln(dev, 3, buf, sizeof buf);
      if (strcmp(buf, "<read>OK") != 0) {
	    fprintf(stderr, "%s: invalid response from "
		    "read command (%s)\n", name, buf);
	    return -1;
      }

      ise_rc = ise_writeln(dev, 3, "get env");

      ise_rc = ise_readln(dev, 3, buf, sizeof buf);
      if (ise_rc != ISE_OK) {
	    fprintf(stderr, "%s: error reading channel 3: %s\n",
		    name, ise_error_msg(ise_rc));
	    return -1;
      }

      while (strcmp(buf, "OK") != 0) {

	    printf("%s\n", buf);

	    ise_rc = ise_readln(dev, 3, buf, sizeof buf);
	    if (ise_rc != ISE_OK) {
		  fprintf(stderr, "%s: error reading channel 3: %s\n",
			  name, ise_error_msg(ise_rc));
		  return -1;
	    }
      }

      printf("OK\n");
      fflush(stdout);
      return 0;
}

static int raw_dump(struct ise_handle*dev, const char*name, unsigned bank)
{
      unsigned idx, jdx;
      ise_error_t ise_rc;
      char buf[128];
      char cookie[64];

      for (idx = 0 ;  idx < 256 ;  idx += 8) {
	    sprintf(buf, "<read%u>get eeprom %u %u %u %u %u %u %u %u %u",
		    idx,bank, idx+0,idx+1,idx+2,idx+3,idx+4,idx+5,idx+6,idx+7);
	    ise_rc = ise_writeln(dev, 3, buf);

	    sprintf(cookie, "<read%u>", idx);

	    printf("%02x:", idx);
	    for (jdx = 0 ;  jdx < 8 ;  jdx += 1) {
		  int val;
		  ise_rc = ise_readln(dev, 3, buf, sizeof buf);
		  if (ise_rc != ISE_OK) {
			fprintf(stderr, "%s: error reading channel 3: %s\n",
				name, ise_error_msg(ise_rc));
			return -1;
		  }

		  if (strncmp(cookie,buf,strlen(cookie)) != 0) {
			fprintf(stderr, "%s: Bad response: %s\n",
				name, buf);
			return -1;
		  }

		  val = strtol(buf+strlen(cookie),0,16);
		  if (isgraph(val))
			printf(" %2c", val);
		  else
			printf(" %02x", val);
	    }
	    printf("\n");
      }
      return 0;
}


/*
 * $Log: ise_getenv.c,v $
 * Revision 1.5  2006/01/24 00:48:54  steve
 *  Add a raw dump mode.
 *
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

