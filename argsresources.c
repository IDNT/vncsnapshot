/*
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
 * argsresources.c - deal with command-line args and resources.
 */
static const char *ID = "$Id: argsresources.c,v 1.8 2006/04/07 03:22:39 grmcdorman Exp $";

#include "vncsnapshot.h"
#include "version.h"

static int setNumber(int *argc, char ***argv, void *arg, int value);
static int setString(int *argc, char ***argv, void *arg, int value);
static int setFlag(int *argc, char ***argv, void *arg, int value);

static char * rect = NULL;

typedef struct {
    const char *optionstring;
    int (*set)(int *argc, char ***argv, void *arg, int value);
    void *arg;
    int   value;
    const char *optiondesc;
} Options;


/* Options - excluding -listen, -tunnel, and -via */
Options cmdLineOptions[] = {
  {"-allowblank",    setFlag,   &appData.ignoreBlank, 0, ": allow blank images"},
  {"-compresslevel", setNumber, &appData.compressLevel, 0, " <COMPRESS-VALUE> (0..9: 0-fast, 9-best)"},
  {"-cursor",        setFlag,   &appData.useRemoteCursor, 1, ": include remote cursor"},
  {"-debug",         setFlag,   &appData.debug, 0, ": enable debug printout"},
  {"-encodings",     setString, &appData.encodingsString, 0, " <ENCODING-LIST> (e.g. \"tight copyrect\")"},
  {"-ignoreblank",   setFlag,   &appData.ignoreBlank, 1, ": ignore blank images"},
  {"-jpeg",          setFlag,   &appData.enableJPEG, 1, ": use JPEG transmission encoding"},
  {"-nocursor",      setFlag,   &appData.useRemoteCursor, 0, ": do not include remote cursor"},
  {"-nojpeg",        setFlag,   &appData.enableJPEG, 0, ": do not use JPEG transmission encoding"},
  {"-passwd",        setString, &appData.passwordFile, 0, " <PASSWD-FILENAME>: read password from file"},
  {"-nullpasswd",      setFlag,   &appData.nullPassword, 1, ": force an empty password"},
  {"-quality",       setNumber, &appData.saveQuality, 0, " <JPEG-QUALITY-VALUE>: output file quality level, percent (0..100)"},
  {"-quiet",         setFlag,   &appData.quiet, 1, ": do not output messages"},
  {"-rect",          setString, &rect, 0, " wxh+x+y: define rectangle to capture (default entire screen)"},
  {"-verbose",       setFlag,   &appData.quiet, 0, ": output messages"},
  {"-vncQuality",    setNumber, &appData.qualityLevel, 0, " <JPEG-QUALITY-VALUE>: transmission quality level (0..9: 0-low, 9-high)"},
  {"-fps",           setNumber, &appData.fps, 0, " <FPS>: Wait <FPS> seconds between snapshots, default 60"},
  {"-count",         setNumber, &appData.count, 0, " <COUNT>: Capture <COUNT> images, default 1"},
  {NULL, NULL, NULL, 0}
};



/*
 * vncServerHost and vncServerPort are set either from the command line or
 * from a dialog box.
 */

char vncServerHost[256];
int vncServerPort = 0;
char *vncServerName;


/*
 * appData is our application-specific data which can be set by the user with
 * application resource specs.  The AppData structure is defined in the header
 * file.
 */

AppData appData = {
    NULL,   /* encodingsString */
    NULL,   /* passwordFile */
    0,      /* nullPassword */
    0,      /* debug */
    4,      /* compressLevel */
    9,      /* qualityLevel */
    -1,     /* useRemoteCursor */
    -1,     /* ignoreBlank */
    -1,     /* enableJPEG */
    100,    /* saveQuality */
    NULL,   /* outputFilename */
    0,      /* quiet */
    0, 0,   /* rectXNegative, rectYNegative */
    0, 0,   /* rect width, height */
    0, 0,   /* rect x, y */
    0,      /* gotCursorPos (-cursor, -nocursor worked) */
    60,     /* fps */
    1,      /* count */
    };


/*
 * removeArgs() is used to remove some of command line arguments.
 */

void
removeArgs(int *argc, char** argv, int idx, int nargs)
{
  int i;
  if ((idx+nargs) > *argc) return;
  for (i = idx+nargs; i < *argc; i++) {
    argv[i-nargs] = argv[i];
  }
  *argc -= nargs;
}

/*
 * usage() prints out the usage message.
 */

void
usage(void)
{
  int i;
  fprintf(stderr,
	  "TightVNC snapshot version " VNC_SNAPSHOT_VERSION " (based on TightVNC 1.2.8 and RealVNC 3.3.7)\n"
	  "\n"
	  "Usage: %s [<OPTIONS>] [<HOST>]:<DISPLAY#> filename\n"
	  "       %s [<OPTIONS>] -listen [<DISPLAY#>] filename\n"
	  "       %s [<OPTIONS>] -tunnel <HOST>:<DISPLAY#> filename\n"
	  "       %s [<OPTIONS>] -via <GATEWAY> [<HOST>]:<DISPLAY#> filename\n"
	  "\n"
	  "<OPTIONS> are:"
	  "\n", programName, programName, programName, programName);
    for (i = 0; cmdLineOptions[i].optionstring; i++) {
        fprintf(stderr, 
	  "        %s", cmdLineOptions[i].optionstring);
        if (cmdLineOptions[i].optiondesc) {
            fprintf(stderr, "%s", cmdLineOptions[i].optiondesc);
        }
        if (cmdLineOptions[i].set == setFlag && *(Bool *)cmdLineOptions[i].arg) {
            fprintf(stderr, " (default)");
        } else if (cmdLineOptions[i].set == setNumber) {
            fprintf(stderr, " (default %d)", *(int *)cmdLineOptions[i].arg);
        } else if (cmdLineOptions[i].set == setString) {
            char *str = *(char **)cmdLineOptions[i].arg;
            if (str != NULL && !*str) {
                fprintf(stderr, " (default %s)", str);
            }
        }
        fprintf(stderr, "\n");
    }
  exit(1);
}


/*
 * GetArgsAndResources() deals with resources and any command-line arguments
 * not already processed by XtVaAppInitialize().  It sets vncServerHost and
 * vncServerPort and all the fields in appData.
 */

void
GetArgsAndResources(int argc, char **argv)
{
  int i;
  int   argsleft;
  char **arg;
  int processed;
  char *vncServerName, *colonPos;
  int len, portOffset;


  argsleft = argc;
  arg = argv+1;

    /* Must have at least one argument */
  if (argc < 2) {
      usage();
  }

  do {
      processed = 0;
      for (i = 0; cmdLineOptions[i].optionstring != NULL; i++) {
          if (strcmp(cmdLineOptions[i].optionstring, arg[0]) == 0) {
              processed = cmdLineOptions[i].set(&argsleft, &arg, cmdLineOptions[i].arg, cmdLineOptions[i].value);
              argsleft--;
              arg++;
          }
      }
  } while (processed);

    /* Parse rectangle provided. */
    if (rect != NULL) {
        /* We could use sscanf, but the return value is not consistent
         * across all platforms.
         */
        long w, h;
        long x, y;
        char *end = NULL;

        w = strtol(rect, &end, 10);
        if (end == NULL || end == rect || *end != 'x') {
            fprintf(stderr, "%s: invalid rectangle specification %s\n",
                    programName, rect);
            usage();
        }
        end++;
        h = strtol(end, &end, 10);
        if (end == NULL || end == rect || (*end != '+' && *end != '-')) {
            fprintf(stderr, "%s: invalid rectangle specification %s\n",
                    programName, rect);
            usage();
        }
        /* determine sign */
        appData.rectXNegative = *end == '-';
        end++;
        x = strtol(end, &end, 10);
        if (end == NULL || end == rect || (*end != '+' && *end != '-')) {
            fprintf(stderr, "%s: invalid rectangle specification %s\n",
                    programName, rect);
            usage();
        }
        /* determine sign */
        appData.rectYNegative = *end == '-';
        end++;
        y = strtol(end, &end, 10);
        if (end == NULL || end == rect || *end != '\0') {
            fprintf(stderr, "%s: invalid rectangle specification %s\n",
                    programName, rect);
            usage();
        }

        appData.rectWidth = w;
        appData.rectHeight = h;
        appData.rectX = x;
        appData.rectY = y;
    }

    argc = argsleft;
    argv = arg;

  /* Check any remaining command-line arguments.  If -listen was specified
     there should be none.  Otherwise the only argument should be the VNC
     server name.  If not given then pop up a dialog box and wait for the
     server name to be entered. */

  if (listenSpecified) {
    if (argc != 2) {
      fprintf(stderr,"\n%s -listen: invalid command line argument: %s\n",
	      programName, argv[0]);
      usage();
    }
    appData.outputFilename = argv[0];
    return;
  }

  if (argc != 3) {
    usage();
    return; /* keep gcc -Wall happy */
  } else {
    vncServerName = argv[0];
    appData.outputFilename = argv[1];

    if (vncServerName[0] == '-')
      usage();
  }

  if (strlen(vncServerName) > 255) {
    fprintf(stderr,"VNC server name too long\n");
    exit(1);
  }

  colonPos = strchr(vncServerName, ':');
  if (colonPos == NULL) {
    /* No colon -- use default port number */
    strcpy(vncServerHost, vncServerName);
    vncServerPort = SERVER_PORT_OFFSET;
  } else {
    memcpy(vncServerHost, vncServerName, colonPos - vncServerName);
    vncServerHost[colonPos - vncServerName] = '\0';
    len = strlen(colonPos + 1);
    portOffset = SERVER_PORT_OFFSET;
    if (colonPos[1] == ':') {
      /* Two colons -- interpret as a port number */
      colonPos++;
      len--;
      portOffset = 0;
    }
    if (!len || strspn(colonPos + 1, "0123456789") != len) {
      usage();
    }
    vncServerPort = atoi(colonPos + 1) + portOffset;
  }
}

static int setNumber(int *argc, char ***argv, void *arg, int value)
{
    long number;
    char *end;
    int ok;

    ok = 0;
    if (*argc > 2) {
        (*argc) --;
        (*argv)++;
        end = NULL;

        number = strtol((*argv)[0], &end, 0);
        if (end != NULL && *end == '\0') {
            *((int *) arg) = number;
            ok = 1;
        }
    }

    return ok;
}

static int setString(int *argc, char ***argv, void *arg, int value)
{
    int ok;

    ok = 0;
    if (*argc > 2) {
        (*argc) --;
        (*argv)++;
        *((char **) arg) = (*argv)[0];
        ok = 1;
    }
    return ok;
}

static int setFlag(int *argc, char ***argv, void *arg, int value)
{
    *((Bool *)arg) = value;
    return 1;
}
