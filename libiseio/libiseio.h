#ifndef __libiseio_H
#define __libiseio_H
/*
 * Copyright (c) 2000-2002 Picture Elements, Inc.
 *    Stephen Williams (steve@picturel.com)
 */
#if !defined(WINNT)
#ident "$Id: libiseio.h,v 1.9 2006/08/10 00:12:56 steve Exp $"
#endif

/*
 * This library supports abstract channel/frame communication with ISE
 * boards in Linux and Windows/NT systems. The library takes care of
 * all the I/O with the ISE board, including manipulating firmware
 * images.
 *
 * This library does *not* interpret command/message strings or frame
 * data that pass between the firmware and the application. It is easy
 * enough to send and receive lines of data, but the firmware
 * documentation should be consulted for what the message mean.
 *
 * -- General Usage --
 *
 * The basic sequence of events starts when the program calls ise_open
 * to open the ISE/SSE board and get a handle. The program then calls
 * ise_restart to load the specific firmware to the board and start
 * the loaded program.
 *
 * After the board is opened and restarted, use ise_channel and
 * ise_frame to create channels and frames defined by the
 * firmware. See the documentation for your specific firmware to learn
 * which channels and frames are needed and why.
 *
 * ise_writeln and ise_readln are then used to communicate firmware
 * commands through the proper channels. The application can use
 * ise_timeout to control blocking behavior on the ise_readln
 * function.
 *
 * When the application is done with the ISE/SSE board, it uses
 * ise_close to close the handle and release the board.
 *
 * -- Threads --
 *
 * The ise_readln and ise_writeln functions are mutually thread
 * safe. That means that a thread can be executing an ise_readln while
 * another thread executes an ise_writeln. Also, ise_readln and
 * ise_writeln to different channels are thread safe. For example, it
 * is OK to ise_readln channel X while another thread executes
 * ise_readln channel Y. It is NOT OK to have multiple threads writing
 * to the same channel, or multiple threads reading from the same
 * channel.
 *
 * However, the remaining functions should not be considered thread
 * safe. All the functions other then ise_readln and ise_writeln
 * should either be executed by a designated thread (i.e. the main
 * thread) or protected by a mutex. These are setup functions, so this
 * should be easy to assure. Also, none of the other functions block
 * indefinitely, so there is no danger of deadlock.
 *
 *
 * -- Logging --
 *
 * The library can be coerced to log diagnostic messages to a file
 * using the environment variable LIBISEIO_LOG. If this variable is
 * unset, then no logging is done. If the environment variable is set,
 * it is assumed to contain the name of a log file. The library will
 * open the the file for append, and will append text to the file. The
 * log file will never be truncated by this library.
 *
 */

# include  <stddef.h>

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN extern
#endif

/*
 * This opaque type represents a single opened ISE device. A pointer
 * to this type is used as a handle for all other ISE board
 * operations.
*/
struct ise_handle;


/*
 */
typedef enum ise_error_code {
      ISE_OK = 0,
      ISE_ERROR = 1,
      ISE_NO_SCOF = 2,
      ISE_NO_CHANNEL = 3,
      ISE_CHANNEL_BUSY = 4,
      ISE_CHANNEL_TIMEOUT = 5
} ise_error_t;

EXTERN const char*ise_error_msg(ise_error_t code);

/*
 * This function returns a handle to the named ISE board. The board is
 * reset, and any open frames are cleared away. No channels are opened
 * yet, as there are other things yet to do that need no channels.
 *
 * The name identifies the board of interest, and has the form "iseN"
 * where N is a decimal number >= 0. For example, "ise0" is the name
 * of the first ISE board in the system.
 *
 * If for some reason the device cannot be opened, ise_open will
 * return NULL.
 */
EXTERN struct ise_handle* ise_open(const char*name);

/*
 * ise_bind is a restricted form of ise_open. The handle that is
 * created does not support ise_restart or ise_prom_version, but can
 * be used by a second process to open an ISE/SSE/JSE board that is
 * already opened by another process.
 *
 * Handles created by ise_bind should only be passed to:
 *  ise_close
 *  ise_channel
 *  ise_timeout
 *  ise_readln
 *  ise_writeln
 *  ise_make_frame
 *  ise_delete_frame.
 *
 * In particular, ise_prom_version and ise_reset CANNOT be used with a
 * handle opened by ise_bind.
 *
 * In general, you do *not* want to use use_bind. Use ise_open unless
 * you know what you are doing.
 */
EXTERN struct ise_handle* ise_bind(const char*name);

/*
 * This function returns a string of detailed ISE FLASH PROM version
 * information. This method can be called any time between the open
 * and close of the handle.
 */
EXTERN const char*ise_prom_version(struct ise_handle*dev);

/*
 * This resets the ISE board, downloads the named firmware, and starts
 * the downloaded program. The function searches the compiled search
 * path for the firmware image so only the name is needed. When this
 * function completes successfully, the board is running the
 * downloaded program.
 *
 * The compiled search path for firmware is:
 *
 *   Linux -- . (current working directory), /usr/share/ise
 *
 *   WinNT -- . (current working directory), then the contents of the
 *            registry entry System\CurrentControlSet\Services\ise\Firmware.
 *
 * The function returns ISE_OK on successful completion.
 */
EXTERN ise_error_t ise_restart(struct ise_handle*dev, const char*firm);

/*
 * This creates channel number <id> to the device. Once the channel is
 * created, it can be used in the writeln and readln functions
 * below. Valid channel numbers range from 0 to 255. The exact content
 * of a particular channel is defined by the firmware loaded by the
 * ise_restart function above.
 */
EXTERN ise_error_t ise_channel(struct ise_handle*dev, unsigned id);

/*
 * The writeln and readln functions write and read lines of text
 * to/from a channel. These functions work with ASCII data. When
 * writing a line, the ise_writeln function terminates the data with
 * its own EOL (of the form appropriate for the ISE board) and writes
 * the entire line to the board.
 *
 * The ise_readln function reads a line of text from the channel, and
 * automatically strips the EOL from the data.
 *
 * The readln and writeln function take care of the newlines so that
 * UNIX/Windows EOL conventions can be ignored. Do not put EOL
 * characters in the output text or expect them in the input text.
 */
EXTERN ise_error_t ise_writeln(struct ise_handle*dev, unsigned channel,
			       const char*data);

EXTERN ise_error_t ise_readln(struct ise_handle*dev, unsigned channel,
			      char*buf, size_t nbuf);

/*
 * Normally, ise_readln will wait as long as necessary (potentially
 * forever) to get the entire line. Use this function to set blocking
 * time limits. The timeout value is given it ms. When a channel is
 * created, it always starts with ISE_TIMEOUT_OFF.
 *
 * If the read_timeout is set to 0, blocking is turned off completely
 * and ise_readln is a polling read.
 *
 * If the read_timeout is set to ISE_TIMEOUT_OFF, then the timeout is
 * turned off, allowing infinite blocking as when the channel is first
 * opened.
 *
 * The ISE_TIMEOUT_FORCE setting doesn't change the timeout settings
 * for a channel, but forces any readln to timeout immediately. This
 * is useful for cancelling an ise_readln set to ISE_TIMEOUT_OFF.
 */
# define ISE_TIMEOUT_OFF (-1)
# define ISE_TIMEOUT_FORCE (-2)
EXTERN ise_error_t ise_timeout(struct ise_handle*dev, unsigned channel,
			       long read_timeout);


/*
 * This function creates and maps a frame. The frame is a chunk of
 * memory that is shared with the ISE board. The dev parameter is the
 * usual handle to the ISE board, the id is a unique (to the handle)
 * id for the frame, and the siz is the requested size of the
 * frame. When the function returns, the siz is replaced with the
 * actual size of the frame, in bytes. 
 *
 * The return value is the base address of the frame, or NULL if there
 * is a problem making the frame.
 *
 * The ise_delete_frame method releases a frame and clears up any
 * resources the frame may have consumed.
 */
EXTERN void* ise_make_frame(struct ise_handle*dev, unsigned id, size_t*siz);

EXTERN void  ise_delete_frame(struct ise_handle*dev, unsigned id);


/*
 * Clean up and close the ISE board. If there are any channels or frames
 * remaining, they are deleted/closed. The ISE board is reset, and the
 * ise_handle object is released.
 */
EXTERN void ise_close(struct ise_handle*dev);

/*
 * This is a function for sending update packages to the jse board.
 */
EXTERN ise_error_t ise_send_package(struct ise_handle*dev, const char*name,
				    const void*pkg_data, size_t ndata,
				    void (*status_msgs)(const char*txt));

#endif
