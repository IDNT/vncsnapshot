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
 * vncsnapshot.c - the VNC snapshot.
 */
static const char *ID = "$Id: vncsnapshot.c,v 1.7 2004/09/09 00:22:33 grmcdorman Exp $";

#include <time.h>
#include <ctype.h>

#include "vncsnapshot.h"

char *programName;

#ifdef WIN32
/* On Win32, sleep() doesn't exist, but Sleep() does, and 
 * takes an argument of milliseconds.
 */
#include <windows.h>    /* for Sleep() */
static void sleep(int seconds)
{
    Sleep(seconds * 1000);
}
#endif

int
main(int argc, char **argv)
{
  int i = 0;
  int count = 1;    /* for multiple snapshots,snapshot number */
  char *filename;   /* output filename; for multiple snapshots, constructed */
  char *cp;         /* work variable */
  char *suffix = NULL; /* suffix to follow snapshot number, including . */
  char *append = NULL; /* point in *filename to put count and suffix */
  time_t last_time = 0; /* value of time() at last snapshot */

  programName = argv[0];

  if (!InitializeSockets()) {
      return 1;
  }

  /* The -listen option is used to make us a daemon process which listens for
     incoming connections from servers, rather than actively connecting to a
     given server. The -tunnel and -via options are useful to create
     connections tunneled via SSH port forwarding. We must test for the
     -listen option before invoking any Xt functions - this is because we use
     forking, and Xt doesn't seem to cope with forking very well. For -listen
     option, when a successful incoming connection has been accepted,
     listenForIncomingConnections() returns, setting the listenSpecified
     flag. */

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-listen") == 0) {
      listenForIncomingConnections(&argc, argv, i);
      break;
    }
    if (strcmp(argv[i], "-tunnel") == 0 || strcmp(argv[i], "-via") == 0) {
      if (!createTunnel(&argc, argv, i))
	exit(1);
      break;
    }
  }

  /* Interpret resource specs and process any remaining command-line arguments
     (i.e. the VNC server name).  If the server name isn't specified on the
     command line, getArgsAndResources() will complain and exit. */

  GetArgsAndResources(argc, argv);

  /* Unless we accepted an incoming connection, make a TCP connection to the
     given VNC server */

  if (!listenSpecified) {
    if (!ConnectToRFBServer(vncServerHost, vncServerPort)) exit(1);
  }

  /* Initialise the VNC connection, including reading the password */

  if (!InitialiseRFBConnection()) exit(1);

  if (!AllocateBuffer()) exit(1);

  /* Tell the VNC server which pixel format and encodings we want to use */

  SendSetPixelFormat();
  SendSetEncodings();


  /* Set up for mutiple images, if required */
  if (appData.count > 1) {
      last_time = time(NULL);
      count = 0;
      /* Maximum length of a 32-bit integer is 10 digits plus sign */
      filename = (char *) malloc(strlen(appData.outputFilename) + 11 + 1);
      /* Determine where to insert number. If the supplied filename
       * ends in .jpg, or .jpeg (case insensitive), then it goes
       * before that. If not, it goes at the end, with .jpg appended.
       */
      cp = strrchr(appData.outputFilename, '.');
      if (cp != NULL) {
          char *jpg = "jpg";
          char *jpeg = "jpeg";
          int is_jpg = 1;
          i = 0;
          while (cp[i+1]) {
              if (tolower(cp[i+1]) != jpg[i]) {
                  is_jpg = 0;
                  break;
              }
              i++;
          }
          if (!is_jpg) {
              i = 0;
              while (cp[i+1]) {
                  if (tolower(cp[i+1]) != jpeg[i]) {
                      cp = NULL;
                      break;
                  }
                  i++;
              }
          }
      }
      if (cp != NULL) {
          strncpy(filename, appData.outputFilename, cp - appData.outputFilename);
          append = filename + (cp - appData.outputFilename);
          suffix = cp;
      } else {
          strcpy(filename, appData.outputFilename);
          suffix = ".jpg";
          append = filename + strlen(filename);
      }
  } else {
      /* Not doing repetitive snapshots. */
      filename = appData.outputFilename;
  }
  /* Grab image; delay and repeat if requested */
  do {
    if(appData.count > 1) {
      sprintf(append, "%05d%s", count, suffix);
      count++;
    }

    /* Now enter the main loop, processing VNC messages. */

    /*
     * Negative X/Y implies from opposite edge.
     */
    if (appData.rectX < 0) {
      appData.rectX = si.framebufferWidth + appData.rectX;
    } else if (appData.rectXNegative) {
      appData.rectX = si.framebufferWidth - appData.rectX - appData.rectWidth;
    }
    if (appData.rectY < 0) {
      appData.rectY = si.framebufferHeight + appData.rectY;
    } else if (appData.rectYNegative) {
      appData.rectY = si.framebufferHeight - appData.rectY - appData.rectHeight;
    }
    if (appData.rectX >= si.framebufferWidth || appData.rectX < 0) {
      fprintf(stderr, "%s: Requested rectangle x <%ld> is outside screen width <%d>, using 0\n",
	      programName, appData.rectX, si.framebufferWidth);
      appData.rectX = 0;
    }
    if (appData.rectY >= si.framebufferHeight || appData.rectY < 0) {
      fprintf(stderr, "%s: Requested rectangle y <%ld> is outside screen height <%d>, using 0\n",
	      programName, appData.rectY, si.framebufferHeight);
      appData.rectY = 0;
    }

    /*
     * Width/height of 0 means to edge.
     */
    if (appData.rectWidth == 0) {
      appData.rectWidth = si.framebufferWidth - appData.rectX;
    }
    if (appData.rectHeight == 0) {
      appData.rectHeight = si.framebufferHeight - appData.rectY;
    }
    if (appData.rectWidth <= 0 || appData.rectWidth > si.framebufferWidth - appData.rectX) {
      fprintf(stderr, "%s: Requested rectangle width <%ld> plus offset <%ld> is wider than screen width <%d>, using %ld\n",
	      programName, appData.rectWidth, appData.rectX, si.framebufferWidth, si.framebufferWidth - appData.rectX);
      appData.rectWidth = si.framebufferWidth - appData.rectX;
    }
    if (appData.rectHeight <= 0 || appData.rectHeight > si.framebufferHeight - appData.rectY) {
      fprintf(stderr, "%s: Requested rectangle height <%ld> plus offset <%ld> is wider than screen height <%d>, using %ld\n",
	      programName, appData.rectHeight, appData.rectY, si.framebufferHeight, si.framebufferHeight - appData.rectY);
      appData.rectHeight = si.framebufferHeight - appData.rectY;
    }
    if (!SendFramebufferUpdateRequest(appData.rectX, appData.rectY, appData.rectWidth,
				      appData.rectHeight, False)) {
      exit(1);
    }

    while (1) {
      if (!HandleRFBServerMessage())
	break;
    }

    /* shrink buffer to requested rectangle */
    ShrinkBuffer(appData.rectX, appData.rectY, appData.rectWidth, appData.rectHeight);
    write_JPEG_file(filename, appData.saveQuality, appData.rectWidth, appData.rectHeight);
    if (!appData.quiet) {
      fprintf(stderr, "Image saved from %s %dx%d screen to ", vncServerName ? vncServerName : "(local host)",
              si.framebufferWidth, si.framebufferHeight);
      if (strcmp(filename, "-") == 0) {
	fprintf(stderr, "- (stdout)");
      } else {
	fprintf(stderr, "%s", filename);
      }
      fprintf(stderr, " using %ldx%ld+%ld+%ld rectangle\n", appData.rectWidth, appData.rectHeight,
              appData.rectX, appData.rectY);
      if (appData.useRemoteCursor != -1 && !appData.gotCursorPos) {
	if (appData.useRemoteCursor) {
	  fprintf(stderr, "Warning: -cursor not supported by server, cursor may not be included in image.\n");
	} else {
	  fprintf(stderr, "Warning: -nocursor not supported by server, cursor may be included in image.\n");
	}
      }
    }

    if (count < appData.count) {
        /* Sleep until next snapshot time rolls around. Note
         * that the period to wait will include the time taken
         * to grab the snapshot.
         */
        time_t now = time(NULL);
        if (now < last_time + appData.fps) {
            sleep(last_time + appData.fps - now);
        }
        last_time = now;
	/* Request update - incremental is fine here. */
	SendIncrementalFramebufferUpdateRequest();
    }
  } while (count < appData.count);

  return 0;
}
