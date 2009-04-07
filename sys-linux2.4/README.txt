
LINUX DRIVER FOR ISE/JSE/EJSE

This directory contains the source code for the Linux driver for the
ISE/JSE/EJSE boards. The source code supports Linux 2.4 and 2.6
kernels, 32bit and 64bit.

* kernel 2.6

Use the Makefile-2.6 makefile for building the modules for 2.6 based
systems. This integrates with the Linux DDK build environment, so the
easiest way to make this work is to link Makefile-2.6 to Makefile and
type make:

  # ln -s Makefile-2.6 Makefile
  # make

To install the module and modprobe hooks, use the Makefile.install
makefile like so:

  # make -f Makefile.install src_install modules_install

* kernel 2.4

To build for 2.4 series kernels, use the Makefile-2.4 makefile.

   # make -f Makefile-2.4

This makes the ise.o file that is the module, compiled for your
current kernel. You can install with the command:

   # make -f Makefile-2.4 modules_install

This will install the module in the correct place for your current
module and will tweak your modules.conf as necessary.

Although not necessary for proper operation, you can also install the
header files and the source code with the command:

  # make -f Makefile-2.4 install
