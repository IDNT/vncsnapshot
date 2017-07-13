/*
 *  Copyright (C) 2000, 2001 Const Kaplinsky.  All Rights Reserved.
 *  Copyright (C) 2000 Tridia Corporation.  All Rights Reserved.
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

/*
 * passwd.c - get password from console, Win32
 */
static const char *ID = "$Id: getpass.c,v 1.3 2002/02/09 22:27:32 grmcdorman Exp $";

#include <conio.h>

static char buffer[512];

char *getpass(const char * prompt)
{
    int ch;
    int index = 0;

    _cputs(prompt);

    ch = _getch();
    while (ch != -1 && ch != '\r') {
        if (ch == 0xE0) {
            _getch();
        } else {
            if (index < 511) {
                buffer[index++] = ch;
            }
        }
        ch = _getch();
    }
    buffer[index] = '\0';
    return buffer;
}

