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

# include  "VideoScopeMain.h"
# include  <QGraphicsScene>
# include  <QGraphicsPixmapItem>
# include  <QString>
# include  <libiseio.h>
# include  <assert.h>

using namespace std;

VideoScopeMain::VideoScopeMain(QWidget*parent)
: QMainWindow(parent), device_(this)
{
      ui.setupUi(this);

      live_display_scene_ = new QGraphicsScene;
      live_display_pixmap_ = new QGraphicsPixmapItem;
      live_display_scene_->addItem(live_display_pixmap_);
      ui.scope_view->setScene(live_display_scene_);

      detect_ise_boards_();
      device_.start();

      connect(ui.attach_check,
	      SIGNAL(stateChanged(int)),
	      SLOT(attach_check_slot_(int)));

	// Connect my attach_board signal to the device_ attach_board
	// slot. This is how I tell the thread to connect to a board.
      connect(this, SIGNAL(attach_board(const QString&)),
	      &device_, SLOT(attach_board(const QString&)));

	// Connect the Live Mode check box directly to the live mode
	// enable slot of the device.
      connect(ui.live_mode_check, SIGNAL(stateChanged(int)),
	      &device_, SLOT(enable_live_mode(int)));

	// Connect the diagjse_version signal from the device thread
	// directly to the scof_version_box widget.
      connect(&device_, SIGNAL(diagjse_version(const QString&)),
	      ui.scof_version_box, SLOT(setText(const QString&)));

	// Receive video_width signals from the device thread.
      connect(&device_, SIGNAL(video0_width(unsigned)),
	      SLOT(video0_width_slot_(unsigned)));
      connect(&device_, SIGNAL(video1_width(unsigned)),
	      SLOT(video1_width_slot_(unsigned)));

	// Receive a live_display chart image from the device.
      connect(&device_, SIGNAL(live_display(const QImage&)),
	      SLOT(live_display_slot_(const QImage&)));
}

VideoScopeMain::~VideoScopeMain()
{
}

void VideoScopeMain::detect_ise_boards_(void)
{
      unsigned idx;

      ui.board_select->clear();

      for (idx = 0 ;  idx < 16 ;  idx += 1) {
	    char path[64];
	    sprintf(path, "ise%u", idx);
	    QString tmp (path);
	    ui.board_select->addItem(tmp);
      }
}

void VideoScopeMain::attach_check_slot_(int state)
{
      if (state) {
	    QString tmp = ui.board_select->currentText();
	    emit attach_board(tmp);
      } else {
	    QString tmp;
	    emit attach_board(tmp);
      }
}

void VideoScopeMain::video0_width_slot_(unsigned wid)
{
      QString tmp ("N/A");
      if (wid != 0) {
	    tmp.setNum(wid);
      }

      ui.measured_width0_box->setText(tmp);
}

void VideoScopeMain::video1_width_slot_(unsigned wid)
{
      QString tmp ("N/A");
      if (wid != 0) {
	    tmp.setNum(wid);
      }

      ui.measured_width1_box->setText(tmp);
}

void VideoScopeMain::live_display_slot_(const QImage&chart)
{
      live_display_pixmap_->setPixmap(QPixmap::fromImage(chart));
      live_display_scene_->update();
}
