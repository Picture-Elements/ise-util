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
# include  <QString>
# include  <libiseio.h>
# include  <assert.h>

using namespace std;

VideoScopeMain::VideoScopeMain(QWidget*parent)
: QMainWindow(parent)
{
      ui.setupUi(this);

      detect_ise_boards_();
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
