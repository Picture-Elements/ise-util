#ifndef __DeviceThread_H
#define __DeviceThread_H
/*
 * Copyright (c) 2009 Picture Elements, Inc.
 *    Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

# include  <QThread>
# include  <QString>
# include  <QTimer>
# include  <QImage>
# include  <libiseio.h>
# include  <stddef.h>

/*
 * Use an instance of the DeviceThread to control a device. The thread
 * receives commands (via its slots) and sends events out via signals.
 */
class DeviceThread  : public QThread {

      Q_OBJECT

    public:
      DeviceThread(QObject*parent =0);
      ~DeviceThread();

    public slots:
	// Send the name of a board that the thread should attach. If
	// the name is empty, then detach from any existing board.
      void attach_board(const QString&name);

	// command to enable/disable live mode.
      void enable_live_mode(int state);

    signals:
	// Signal the scof version string when the board is loaded.
      void diagjse_version(const QString&text);
	// Signal the measured video width.
      void video0_width(unsigned wid);
      void video1_width(unsigned wid);
	// Signal that a new live-display is ready.
      void live_display(const QImage&chart);

    private slots:
      void clock_slot_(void);

    private:
      void run();
      void activate_live_mode_(bool);

    private:
      struct ise_handle* dev_;
      void*dev_frame_;
      size_t dev_frame_size_;

      char buf_[4096];

      QImage chart_;
      unsigned long red_line_[64*1024];
      unsigned long gre_line_[64*1024];
      unsigned long blu_line_[64*1024];

	// Measured video width, or 0 if not measured yet.
      unsigned video0_width_;
      unsigned video1_width_;
      bool live_flag_;

      QTimer clock_;
};

#endif
