//  Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//
//  This file is part of the VNC system.
//
//  The VNC system is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
//  USA.
//
// TightVNC distribution homepage on the Web: http://www.tightvnc.com/
//
// If the source code for the VNC system is not available from the place whence 
// you received this file, check http://www.uk.research.att.com/vnc or contact
// the authors on vnc@uk.research.att.com for information on obtaining it.


/*
 * vncauth.c - Functions for VNC password management and authentication.
 */
static const char *ID = "$Id: vncauth.c,v 1.5 2004/09/09 00:22:33 grmcdorman Exp $";

#ifndef WIN32
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "stdhdrs.h"
#include "vncauth.h"
#include "d3des.h"
#include <memory.h>

/*
 * We use a fixed key to store passwords, since we assume that our local
 * file system is secure but nonetheless don't want to store passwords
 * as plaintext.
 */

unsigned char fixedkey[8] = {23,82,107,6,35,78,88,7};


/*
 * Encrypt a password and store it in a file.  Returns 0 if successful,
 * 1 if the file could not be written.
 */

int
vncEncryptAndStorePasswd(char *passwd, char *fname)
{
    FILE *fp;
    int i;
    unsigned char encryptedPasswd[8];

    if ((fp = fopen(fname,"wb")) == NULL) return 1;

#ifndef WIN32
    chmod(fname, S_IRUSR|S_IWUSR);
#endif

    /* pad password with nulls */

    for (i = 0; i < 8; i++) {
	if (i < strlen(passwd)) {
	    encryptedPasswd[i] = passwd[i];
	} else {
	    encryptedPasswd[i] = 0;
	}
    }

    /* Do encryption in-place - this way we overwrite our copy of the plaintext
       password */

    deskey(fixedkey, EN0);
    des(encryptedPasswd, encryptedPasswd);

    for (i = 0; i < 8; i++) {
	putc(encryptedPasswd[i], fp);
    }
  
    fclose(fp);
    return 0;
}

/*
 * Decrypt a password from a file.  Returns a pointer to a newly allocated
 * string containing the password or a null pointer if the password could
 * not be retrieved for some reason.
 */

char *
vncDecryptPasswdFromFile(char *fname)
{
    FILE *fp;
    int i, ch;
    unsigned char *passwd = (unsigned char *)malloc(9);

    if ((fp = fopen(fname,"r")) == NULL) return NULL;

    for (i = 0; i < 8; i++) {
	ch = getc(fp);
	if (ch == EOF) {
	    fclose(fp);
	    return NULL;
	}
	passwd[i] = ch;
    }

    fclose(fp);

    deskey(fixedkey, DE1);
    des(passwd, passwd);

    passwd[8] = 0;

    return (char *)passwd;
}


/*
 * Encrypt CHALLENGESIZE bytes in memory using a password.
 */

void
vncEncryptBytes(unsigned char *bytes, char *passwd)
{
    unsigned char key[8];
    unsigned int i;

    /* key is simply password padded with nulls */

    for (i = 0; i < 8; i++) {
	if (i < strlen(passwd)) {
	    key[i] = passwd[i];
	} else {
	    key[i] = 0;
	}
    }

    deskey(key, EN0);

    for (i = 0; i < CHALLENGESIZE; i += 8) {
		des(bytes+i, bytes+i);
    }
}


/*
 * Encrypt a password into the specified space.
 * encryptedPasswd will be 8 bytes long - sufficient space 
 *   should be allocated.
 */

void
vncEncryptPasswd( unsigned char *encryptedPasswd, char *passwd )
{
	unsigned int i;

    /* pad password with nulls */
    for (i = 0; i < MAXPWLEN; i++) {
		if (i < strlen(passwd)) {
			encryptedPasswd[i] = passwd[i];
		} else {	
			encryptedPasswd[i] = 0;
		}
    }

    /* Do encryption in-place - this way we overwrite our copy of the plaintext
       password */
    deskey(fixedkey, EN0);
    des(encryptedPasswd, encryptedPasswd);
}


/*
 * Decrypt a password.  Returns a pointer to a newly allocated
 * string containing the password or a null pointer if the password could
 * not be retrieved for some reason.
 */

char *
vncDecryptPasswd(const unsigned char *encryptedPasswd)
{
    unsigned int i;
    unsigned char *passwd = (unsigned char *)malloc(MAXPWLEN+1);

	memcpy(passwd, encryptedPasswd, MAXPWLEN);

    for (i = 0; i < MAXPWLEN; i++) {
		passwd[i] = encryptedPasswd[i];
    }

    deskey(fixedkey, DE1);
    des(passwd, passwd);

    passwd[MAXPWLEN] = 0;

    return (char *)passwd;
}


