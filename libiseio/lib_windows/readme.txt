
ISE API FOR WINNT

The libiseio.dll object is built from the source in here. This compile
is tested with Mingw-3.1.0, but should work with any recent MinGW
install. Compile the library with the command

    make -f makefile.mingw

to make the libiseio.dll and libiseio.lib files.

INSTALL

Install the runtime by copying the libiseio.dll file to the
\winnt\system32 directory. Presumably, the ise.sys device driver is
already installed.

The developer support includes the files libiseio.lib, libiseio.a and
..\libiseio.h. Copy these files into some convenient location where
the user can find them to link with their programs. The developer
support files are suitable for use in MSVC class compilers or MinGW
compilers.
