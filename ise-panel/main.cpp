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
#ident "$Id: main.cpp,v 1.1 2008/10/07 18:37:00 steve-icarus Exp $"


# include  <qapplication.h>
# include  "IsePanelMain.h"

int main(int argc, char*argv[])
{
      QApplication app(argc, argv);

      IsePanelMain *widget = new IsePanelMain;
      widget->show();

      return app.exec();
}
/*
 * $Log: main.cpp,v $
 * Revision 1.1  2008/10/07 18:37:00  steve-icarus
 *  Add QT version of ies panel
 *
 */

