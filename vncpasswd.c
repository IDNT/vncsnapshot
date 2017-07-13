/*
 *  Copyright (C) 2002 RealVNC Ltd.
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
 *  vncpasswd:  A standalone program which gets and verifies a password, 
 *              encrypts it, and stores it to a file.  Always ignore anything
 *              after 8 characters, since this is what Solaris getpass() does
 *              anyway.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#else
#include <conio.h>
#endif

#include "vncauth.h"

#define MINPASSWD 6

#ifdef WIN32
static char *vnc_getpass(const char *prompt);
#else
/* *nix systems can use the system getpass() function. */
#define vnc_getpass(prompt) getpass(prompt)
#endif

static void usage(char *argv[]) {
#ifdef WIN32
  fprintf(stderr,"Usage: %s file\n",argv[0]);
#else
  fprintf(stderr,"Usage: %s [file]\n",argv[0]);
#endif
  exit(1);
}

int main(int argc, char *argv[]) {
  char *passwd;
  char *passwd1;
  char passwdFile[256];
  int i;

#ifndef WIN32
    /* Home directory only applies on *nix systems */
  if (argc == 1) {
      if (getenv("HOME") == NULL) {
	  fprintf(stderr,"Error: no HOME environment variable\n");
	  exit(1);
      }
      sprintf(passwdFile,"%s/.vnc/passwd",getenv("HOME"));

  } else
#endif
      if (argc == 2) {

      strcpy(passwdFile,argv[1]);

  } else {
      usage(argv);
  }

  while (1) {  
    passwd = vnc_getpass("Password: ");
    if (!passwd) {
      fprintf(stderr,"Can't get password: not a tty?\n");
      exit(1);
    }   
    if (strlen(passwd) < MINPASSWD) {
      fprintf(stderr,"Password too short\n");
      exit(1);
    }   
    if (strlen(passwd) > MAXPWLEN) {
      fprintf(stderr, "Warning: password truncated to 8 characters\n");
      passwd[MAXPWLEN] = '\0';
    }

    passwd1 = strdup(passwd);

    passwd = vnc_getpass("Verify:   ");
    if (strlen(passwd) > MAXPWLEN) {
      passwd[MAXPWLEN] = '\0';
    }

    if (strcmp(passwd1, passwd) == 0) {
      if (vncEncryptAndStorePasswd(passwd, passwdFile) != 0) {
	fprintf(stderr,"Cannot write password file %s\n",passwdFile);
	exit(1);
      }
      for (i = 0; i < strlen(passwd); i++)
	passwd[i] = passwd1[i] = '\0';
      return 0;
    }

    fprintf(stderr,"They don't match. Try again.\n\n");
  }
}


#ifdef WIN32
char * vnc_getpass(const char *prompt)
{
    register char *p;
    register c;
    static char pbuf[50+1];
    
    fprintf(stderr, "%s", prompt); (void) fflush(stderr);
    for (p=pbuf; (c = _getch())!=13 && c!=EOF;) {
        if (p < &pbuf[sizeof(pbuf)-1])
          *p++ = c;
    }
    *p = '\0';
    fprintf(stderr, "\n"); (void) fflush(stderr);
    return(pbuf);
}
#endif
