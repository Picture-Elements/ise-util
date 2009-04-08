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

# include  "DeviceThread.h"
# include  <QTimer>
# include  <QStringList>
# include  <assert.h>

DeviceThread::DeviceThread(QObject*parent)
: QThread(parent), clock_(parent)
{
      dev_ = 0;
      video_width_ = 0;
      live_flag_ = false;
      clock_.setSingleShot(false);
}

DeviceThread::~DeviceThread()
{
      assert(dev_ == 0);
}

void DeviceThread::run()
{
      connect(&clock_, SIGNAL(timeout()), this, SLOT(clock_slot_()));

      exec();
}

void DeviceThread::attach_board(const QString&text)
{
      ise_error_t ise_rc;

      if (dev_ != 0) {
	    ise_close(dev_);
	    dev_ = 0;
      }

      if (text.isEmpty()) {
	    emit diagjse_version(QString("no device"));
	    return;
      }

      dev_ = ise_open(text.toStdString().c_str());
      if (dev_ == 0) {
	    emit diagjse_version(QString("no device"));
	    return;
      }

      ise_rc = ise_restart(dev_, "diagjse");
      if (ise_rc != ISE_OK) {
	    emit diagjse_version(QString("no diagjse"));
	    ise_close(dev_);
	    dev_ = 0;
	    return;
      }

      ise_rc = ise_channel(dev_, 3);
      assert(ise_rc == ISE_OK);

	// Get the diagjse version string
      ise_writeln(dev_, 3, "<ver>get version");
      ise_readln(dev_, 3, buf_, sizeof buf_);

	// Chop off the cookie
      { QString line (buf_);
	if (line.startsWith(QString("<ver>")))
	      line.remove(0,5);

	emit diagjse_version(line);
      }

	// Start out not knowing the width of the video. Set the video
	// width to 0 and signal this empty width.
      video_width_ = 0;
      emit video_width(0);

	// Start a timer to query the geometry. It is enough to query
	// every 200ms.
      clock_.start(200);
}

void DeviceThread::activate_live_mode_(bool flag)
{
      if (flag) {
	    ise_writeln(dev_, 3, "set video-flag live");
	    ise_readln(dev_, 3, buf_, sizeof buf_);
	    clock_.start(300);
      } else {
	    ise_writeln(dev_, 3, "set video-flag page");
	    ise_readln(dev_, 3, buf_, sizeof buf_);
	    clock_.stop();
      }
}

void DeviceThread::enable_live_mode(int state)
{
      bool flag = state != 0;
      if (flag == live_flag_)
	    return;

      if (flag) {
	    live_flag_ = true;
	    if (video_width_ == 0)
		  return;

	    if (dev_)
		  activate_live_mode_(true);

      } else {
	    live_flag_ = false;
	    if (dev_)
		  activate_live_mode_(false);
      }
}

void DeviceThread::clock_slot_(void)
{
      if (dev_ == 0) {
	    clock_.stop();
	    return;
      }

	// If the clock is ticking and I have no video width, then the
	// tick is saying it is time to try again to query the geometry.
      if (video_width_ == 0) {

	      // Send the "get geometry" command
	    ise_writeln(dev_, 3, "<geo>get geometry");
	    ise_readln(dev_, 3, buf_, sizeof buf_);

	      // The response must have the right cookie.
	    QString line (buf_);
	    if (!line.startsWith(QString("<geo>")))
		  return;

	      // Remove the cookie from the response.
	    line.remove(0, 5);

	      // Split the geometry response into tokens and interpret
	      // them one-by-one. We are principally interrested in
	      // the "width=<n>" token here.
	    QStringList tokens = line.split(QString(" "),
					    QString::SkipEmptyParts);
	    while (tokens.size() > 0) {
		  QString word = tokens.takeFirst();

		  if (word.startsWith(QString("width="))) {
			word.remove(0, 6);
			video_width_ = word.toUInt();
			emit video_width(video_width_);
		  }
	    }

	      // If we got the video width, then stop the clock.
	    if (video_width_ != 0) {
		  activate_live_mode_(live_flag_);
	    }
      }

      if (live_flag_) {
	    ise_writeln(dev_, 3, "get readable");
	    ise_readln(dev_, 3, buf_, sizeof buf_);

	    QString line (buf_);
	    if (line.toUInt() > 0) {
		  ise_writeln(dev_, 3, "read 0");
		  ise_readln(dev_, 3, buf_, sizeof buf_);

		  printf("XXXX video: %s\n", buf_);
	    }
      }
}
