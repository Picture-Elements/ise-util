#ifndef __VideoScopeMain_H
#define __VideoScopeMain_H
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

# include  <qapplication.h>
# include  "ui_video_scope.h"
# include  "DeviceThread.h"

class QGraphicsScene;
class QGraphicsPixmapItem;

class VideoScopeMain  : public QMainWindow {

      Q_OBJECT

    public:
      VideoScopeMain(QWidget*parent =0);
      ~VideoScopeMain();

    private:
      void detect_ise_boards_(void);

    private slots:
      void attach_check_slot_(int state);
      void video0_width_slot_(unsigned wid);
      void video1_width_slot_(unsigned wid);
      void live_display_slot_(const QImage&chart);

    signals:
      void attach_board(const QString&);

    private:
      Ui::VideoScope ui;
      QGraphicsScene*live_display_scene_;
      QGraphicsPixmapItem*live_display_pixmap_;
      DeviceThread device_;
};

#endif
