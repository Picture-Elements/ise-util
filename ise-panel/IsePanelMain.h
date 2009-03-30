#ifndef __IsePanelMain_H
#define __IsePanelMain_H
/*
 * Copyright (c) 2008 Picture Elements, Inc.
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
#ident "$Id: IsePanelMain.h,v 1.3 2008/10/08 16:10:14 steve-icarus Exp $"

# include  <qapplication.h>
# include  "ui_ise_panel.h"

class IsePanelMain  : public QMainWindow {

      Q_OBJECT

    public:
      IsePanelMain(QWidget*parent =0);
      ~IsePanelMain();

    private:
      void closeEvent(QCloseEvent*event);
      void detect_ise_boards_(void);
      void open_selected_isex_(void);

    private slots:
      void board_select_slot_(int);
      void console_refresh_slot_(void);
      void diag0_slot_(void);
      void diag1_slot_(void);
      void power_switch_slot_(int state);

    private:
      Ui::IsePanel ui;
      bool power_off_;
      int ise_fdx_;
      int ise_cons_;
};

/*
 * $Log: IsePanelMain.h,v $
 * Revision 1.3  2008/10/08 16:10:14  steve-icarus
 *  Prevent close underneath powered off board.
 *
 * Revision 1.2  2008/10/08 00:08:37  steve-icarus
 *  Add DIAG buttons.
 *
 * Revision 1.1  2008/10/07 18:37:00  steve-icarus
 *  Add QT version of ies panel
 *
 */
#endif
