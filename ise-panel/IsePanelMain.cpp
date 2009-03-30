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
#ident "$Id: IsePanelMain.cpp,v 1.6 2008/10/15 18:34:44 steve-icarus Exp $"

# include  "IsePanelMain.h"
# include  <QCloseEvent>
# include  <QMessageBox>
# include  <sys/types.h>
# include  <sys/stat.h>
# include  <sys/ioctl.h>
# include  <fcntl.h>
# include  <ucrif.h>
# include  <errno.h>
# include  <assert.h>

using namespace std;

#define CONSOLE_PATH "/proc/driver/isecons"

IsePanelMain::IsePanelMain(QWidget*parent)
: QMainWindow(parent)
{
      ui.setupUi(this);

      power_off_ = false;
      ise_fdx_ = -1;
      ise_cons_ = -1;
      detect_ise_boards_();

      connect(ui.board_select,
	      SIGNAL(currentIndexChanged(int)),
	      SLOT(board_select_slot_(int)));

      connect(ui.console_refresh,
	      SIGNAL(clicked()),
	      SLOT(console_refresh_slot_()));

      connect(ui.diag0,
	      SIGNAL(clicked()),
	      SLOT(diag0_slot_()));

      connect(ui.diag1,
	      SIGNAL(clicked()),
	      SLOT(diag1_slot_()));

      connect(ui.power_switch,
	      SIGNAL(stateChanged(int)),
	      SLOT(power_switch_slot_(int)));
}

IsePanelMain::~IsePanelMain()
{
}

void IsePanelMain::closeEvent(QCloseEvent*event)
{
      if (power_off_) {
	    event->ignore();
	    QMessageBox::information(this, "Can't close yet",
				     "Board is powered off. Power it back on"
				     " before quitting.");
	    return;
      }

      event->accept();
}

void IsePanelMain::detect_ise_boards_(void)
{
      unsigned idx;

      ui.board_select->clear();

      for (idx = 0 ;  idx < 16 ;  idx += 1) {
	    char path[64];
	    int fd;
	    snprintf(path, sizeof path, "/dev/isex%u", idx);

	    fd = ::open(path, O_RDWR, 0);
	    if (fd < 0)
		  break;

	    ::close(fd);

	    snprintf(path, sizeof path, "ise%u", idx);
	    fprintf(stderr, "Detected %s\n", path);
	    ui.board_select->addItem(path);
      }
}

void IsePanelMain::open_selected_isex_(void)
{
      QString ise = ui.board_select->currentText();
      char path[128];
      int idx;
      int rc;

      rc = sscanf(ise.toAscii(), "ise%d", &idx);
      assert(rc == 1);

      sprintf(path, "/dev/isex%d", idx);
      ise_fdx_ = ::open(path, O_RDWR, 0);
}

void IsePanelMain::board_select_slot_(int)
{
      if (ise_fdx_ >= 0) {
	    ::close(ise_fdx_);
	    ise_fdx_ = -1;
      }
}

void IsePanelMain::console_refresh_slot_(void)
{
      char buf[512];
      int rc;

      if (ise_cons_ == -1) {
	    ise_cons_ = ::open(CONSOLE_PATH, O_RDONLY, 0);
	    if (ise_cons_ == -1) {
		  fprintf(stderr, "Unable to open %s\n",
			  CONSOLE_PATH);
		  return;
	    }
      }

      QString text;
      do {
	    rc = ::read(ise_cons_, buf, sizeof buf-1);
	    if (rc <= 0)
		  break;
	    buf[rc] = 0;
	    text.append(buf);
      } while (rc == (sizeof buf-1));

      ui.console_text->append(text);
}

void IsePanelMain::diag0_slot_(void)
{
      open_selected_isex_();
      int rc = ioctl(ise_fdx_, UCRX_DIAGNOSE, 0);
      if (rc < 0) {
	    char buf[128];
	    snprintf(buf, sizeof buf, "Error (%d) invoking diagnose-0", errno);
	    QMessageBox::information(this, "Diagnose 0", buf);
      }
      console_refresh_slot_();
}

void IsePanelMain::diag1_slot_(void)
{
      open_selected_isex_();
      int rc = ioctl(ise_fdx_, UCRX_DIAGNOSE, 1);
      if (rc < 0) {
	    char buf[128];
	    snprintf(buf, sizeof buf, "Error (%d) invoking diagnose-1", errno);
	    QMessageBox::information(this, "Diagnose 0", buf);
      }
      console_refresh_slot_();
}

void IsePanelMain::power_switch_slot_(int state)
{
      if (state) {
	    open_selected_isex_();
	    int rc = ::ioctl(ise_fdx_, UCRX_REMOVE, 0);
	    if (rc < 0) {
		  char buf[128];
		  snprintf(buf, sizeof buf, "Error (%d) turning power off.", errno);
		  QMessageBox::information(this, "Power Switch", buf);

		  ::close(ise_fdx_);
		  ise_fdx_ = -1;
		  ui.power_switch->setCheckState(Qt::Unchecked);

	    } else {
		  power_off_ = true;
		  ui.board_select->setEnabled(false);
	    }

      } else {
	    ui.board_select->setEnabled(true);

	      // If we are going from power-off to power on, then send
	      // the ioctl to restore power. Note that it is possible
	      // for this to be not the case, if for example the box
	      // is unchecked by error handling above.
	    if (power_off_) {
		  power_off_ = false;
		  assert(ise_fdx_ >= 0);
		  int rc = ::ioctl(ise_fdx_, UCRX_REPLACE, 0);
		  assert(rc >= 0);
	    }

	    if (ise_fdx_ >= 0) {
		  ::close(ise_fdx_);
		  ise_fdx_ = -1;
	    }
      }
}

/*
 * $Log: IsePanelMain.cpp,v $
 * Revision 1.6  2008/10/15 18:34:44  steve-icarus
 *  Handle errors turning power on/off.
 *
 * Revision 1.5  2008/10/08 16:10:14  steve-icarus
 *  Prevent close underneath powered off board.
 *
 * Revision 1.4  2008/10/08 04:15:17  steve-icarus
 *  Replace some assertions with some error messages.
 *
 * Revision 1.3  2008/10/08 00:35:35  steve-icarus
 *  Clean up text append to console.
 *
 * Revision 1.2  2008/10/08 00:08:37  steve-icarus
 *  Add DIAG buttons.
 *
 * Revision 1.1  2008/10/07 18:37:00  steve-icarus
 *  Add QT version of ies panel
 *
 */

