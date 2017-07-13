/*
 *  Copyright (C) 2002 RealVNC Ltd.
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
 * rfbproto.c - functions to deal with client side of RFB protocol.
 */
static const char *ID = "$Id: rfbproto.c,v 1.7 2006/04/07 03:22:39 grmcdorman Exp $";

#ifdef WIN32
#include "vncauth.h"

#define strcasecmp(a,b) stricmp(a,b)
#define strncasecmp(a,b,n) strnicmp(a, b, n)

#else
#include <unistd.h>
#include <pwd.h>
#endif

#include <errno.h>

#include "vncsnapshot.h"
#include "vncauth.h"

#define INT16 jpegINT16
#include <zlib.h>
#include <jpeglib.h>
#undef INT16

/* do not need non-32 bit versions of these */
static Bool HandleRRE32(int rx, int ry, int rw, int rh);
static Bool HandleCoRRE32(int rx, int ry, int rw, int rh);
static Bool HandleHextile32(int rx, int ry, int rw, int rh);
static Bool HandleZlib32(int rx, int ry, int rw, int rh);
static Bool HandleTight32(int rx, int ry, int rw, int rh);

static long ReadCompactLen (void);

/* JPEG */
static void JpegInitSource(j_decompress_ptr cinfo);
static boolean JpegFillInputBuffer(j_decompress_ptr cinfo);
static void JpegSkipInputData(j_decompress_ptr cinfo, long num_bytes);
static void JpegTermSource(j_decompress_ptr cinfo);
static void JpegSetSrcManager(j_decompress_ptr cinfo, CARD8 *compressedData,
                              int compressedLen);

char *desktopName;

rfbPixelFormat myFormat;
Bool pendingFormatChange = False;

/*
 * Macro to compare pixel formats.
 */

#define PF_EQ(x,y)							\
	((x.bitsPerPixel == y.bitsPerPixel) &&				\
	 (x.depth == y.depth) &&					\
	 ((x.bigEndian == y.bigEndian) || (x.bitsPerPixel == 8)) &&	\
	 (x.trueColour == y.trueColour) &&				\
	 (!x.trueColour || ((x.redMax == y.redMax) &&			\
			    (x.greenMax == y.greenMax) &&		\
			    (x.blueMax == y.blueMax) &&			\
			    (x.redShift == y.redShift) &&		\
			    (x.greenShift == y.greenShift) &&		\
			    (x.blueShift == y.blueShift))))

int currentEncoding = rfbEncodingZRLE;
Bool pendingEncodingChange = False;
int supportedEncodings[] = {
  rfbEncodingZRLE, rfbEncodingHextile, rfbEncodingCoRRE, rfbEncodingRRE,
  rfbEncodingRaw, rfbEncodingTight
};
#define NUM_SUPPORTED_ENCODINGS (sizeof(supportedEncodings)/sizeof(int))

rfbServerInitMsg si;
char *serverCutText = NULL;
Bool newServerCutText = False;

int endianTest = 1;


/* note that the CoRRE encoding uses this buffer and assumes it is big enough
   to hold 255 * 255 * 32 bits -> 260100 bytes.  640*480 = 307200 bytes */
/* also hextile assumes it is big enough to hold 16 * 16 * 32 bits */
#define BUFFER_SIZE (640*480)
static char buffer[BUFFER_SIZE];


/* The zlib encoding requires expansion/decompression/deflation of the
   compressed data in the "buffer" above into another, result buffer.
   However, the size of the result buffer can be determined precisely
   based on the bitsPerPixel, height and width of the rectangle.  We
   allocate this buffer one time to be the full size of the buffer. */

static int raw_buffer_size = -1;
static char *raw_buffer;

static z_stream decompStream;
static Bool decompStreamInited = False;


/*
 * Variables for the ``tight'' encoding implementation.
 */

/* Separate buffer for compressed data. */
#define ZLIB_BUFFER_SIZE 512
static char zlib_buffer[ZLIB_BUFFER_SIZE];

/* Four independent compression streams for zlib library. */
static z_stream zlibStream[4];
static Bool zlibStreamActive[4] = {
  False, False, False, False
};

/* Filter stuff. Should be initialized by filter initialization code. */
static Bool cutZeros;
static int rectWidth, rectColors;
static char tightPalette[256*4];
static CARD8 tightPrevRow[2048*3*sizeof(CARD16)];

/* JPEG decoder state. */
static Bool jpegError;


/*
 * InitialiseRFBConnection.
 */

Bool
InitialiseRFBConnection()
{
  rfbProtocolVersionMsg pv;
  int major,minor;
  CARD32 authScheme, reasonLen, authResult;
  char *reason;
  CARD8 challenge[CHALLENGESIZE];
  char *passwd;
  rfbClientInitMsg ci;

  if (!ReadFromRFBServer(pv, sz_rfbProtocolVersionMsg)) return False;

  pv[sz_rfbProtocolVersionMsg] = 0;

  if (sscanf(pv,rfbProtocolVersionFormat,&major,&minor) != 2) {
    fprintf(stderr,"Not a valid VNC server\n");
    return False;
  }

  if (!appData.quiet) {
     fprintf(stderr,"VNC server supports protocol version %d.%d (viewer %d.%d)\n",
             major, minor, rfbProtocolMajorVersion, rfbProtocolMinorVersion);
  }

  major = rfbProtocolMajorVersion;
  minor = rfbProtocolMinorVersion;

  sprintf(pv,rfbProtocolVersionFormat,major,minor);

  if (!WriteToRFBServer(pv, sz_rfbProtocolVersionMsg)) return False;
  if (!ReadFromRFBServer((char *)&authScheme, 4)) return False;

  authScheme = Swap32IfLE(authScheme);

  switch (authScheme) {

  case rfbConnFailed:
    if (!ReadFromRFBServer((char *)&reasonLen, 4)) return False;
    reasonLen = Swap32IfLE(reasonLen);

    reason = malloc(reasonLen);

    if (!ReadFromRFBServer(reason, reasonLen)) return False;

    fprintf(stderr,"VNC connection failed: %.*s\n",(int)reasonLen, reason);
    return False;

  case rfbNoAuth:
    if (!appData.quiet) {
      fprintf(stderr,"No authentication needed\n");
    }
    break;

  case rfbVncAuth:
    if (!ReadFromRFBServer((char *)challenge, CHALLENGESIZE)) return False;

    /* If -nullpassword was specified, supply just that */
    if (appData.nullPassword) {
      passwd = "";
    } else if (appData.passwordFile) {
      passwd = vncDecryptPasswdFromFile(appData.passwordFile);
      if (!passwd) {
	fprintf(stderr,"Cannot read valid password from file \"%s\"\n",
		appData.passwordFile);
	return False;
      }
    } else {
      passwd = getpass("Password: ");
    }

    if (passwd == NULL) {
      fprintf(stderr,"Reading password failed\n");
      return False;
    }
    if (!appData.nullPassword && strlen(passwd) == 0) {
      fprintf(stderr, "Warning: null password provided, proceeding with authetication\n");
    }
    if (strlen(passwd) > 8) {
      passwd[8] = '\0';
    }

    vncEncryptBytes(challenge, passwd);

	/* Lose the password from memory */
    if (!appData.nullPassword) memset(passwd, '\0', strlen(passwd));

    if (!WriteToRFBServer((char *)challenge, CHALLENGESIZE)) return False;

    if (!ReadFromRFBServer((char *)&authResult, 4)) return False;

    authResult = Swap32IfLE(authResult);

    switch (authResult) {
    case rfbVncAuthOK:
      if (!appData.quiet) {
        fprintf(stderr,"VNC authentication succeeded\n");
      }
      break;
    case rfbVncAuthFailed:
      fprintf(stderr,"VNC authentication failed\n");
      return False;
    case rfbVncAuthTooMany:
      fprintf(stderr,"VNC authentication failed - too many tries\n");
      return False;
    default:
      fprintf(stderr,"Unknown VNC authentication result: %d\n",
	      (int)authResult);
      return False;
    }
    break;

  default:
    fprintf(stderr,"Unknown authentication scheme from VNC server: %d\n",
	    (int)authScheme);
    return False;
  }

  ci.shared = 1;

  if (!WriteToRFBServer((char *)&ci, sz_rfbClientInitMsg)) return False;

  if (!ReadFromRFBServer((char *)&si, sz_rfbServerInitMsg)) return False;

  si.framebufferWidth = Swap16IfLE(si.framebufferWidth);
  si.framebufferHeight = Swap16IfLE(si.framebufferHeight);
  si.format.redMax = Swap16IfLE(si.format.redMax);
  si.format.greenMax = Swap16IfLE(si.format.greenMax);
  si.format.blueMax = Swap16IfLE(si.format.blueMax);
  si.nameLength = Swap32IfLE(si.nameLength);

  desktopName = malloc(si.nameLength + 1);
  if (!desktopName) {
    fprintf(stderr, "Error allocating memory for desktop name, %lu bytes\n",
            (unsigned long)si.nameLength);
    return False;
  }

  if (!ReadFromRFBServer(desktopName, si.nameLength)) return False;

  desktopName[si.nameLength] = 0;

  if (!appData.quiet) {
    fprintf(stderr,"Desktop name \"%s\"\n",desktopName);

    fprintf(stderr,"Connected to VNC server, using protocol version %d.%d\n",
            rfbProtocolMajorVersion, rfbProtocolMinorVersion);

    fprintf(stderr,"VNC server default format:\n");
    PrintPixelFormat(&si.format);
  }

  return True;
}


Bool SendFramebufferUpdateRequest(int x, int y, int w, int h, Bool incremental)
{
  rfbFramebufferUpdateRequestMsg fur;

  fur.type = rfbFramebufferUpdateRequest;
  fur.incremental = incremental ? 1 : 0;
  fur.x = Swap16IfLE(x);
  fur.y = Swap16IfLE(y);
  fur.w = Swap16IfLE(w);
  fur.h = Swap16IfLE(h);

  if (!WriteToRFBServer((char *)&fur, sz_rfbFramebufferUpdateRequestMsg))
    return False;

  return True;
}


Bool SendSetPixelFormat()
{
  rfbSetPixelFormatMsg spf;

  spf.type = rfbSetPixelFormat;
  spf.format = myFormat;
  spf.format.redMax = Swap16IfLE(spf.format.redMax);
  spf.format.greenMax = Swap16IfLE(spf.format.greenMax);
  spf.format.blueMax = Swap16IfLE(spf.format.blueMax);
    PrintPixelFormat(&myFormat);

  return WriteToRFBServer((char *)&spf, sz_rfbSetPixelFormatMsg);
}


Bool SendSetEncodings()
{
  char buf[sz_rfbSetEncodingsMsg + MAX_ENCODINGS * 4];
  rfbSetEncodingsMsg *se = (rfbSetEncodingsMsg *)buf;
  CARD32 *encs = (CARD32 *)(&buf[sz_rfbSetEncodingsMsg]);
  int len = 0;
  int i;
  Bool requestCompressLevel = False;
  Bool requestQualityLevel = False;
  Bool requestLastRectEncoding = False;

  se->type = rfbSetEncodings;
  se->nEncodings = 0;

  if (appData.encodingsString) {
    char *encStr = appData.encodingsString;
    int encStrLen;
    do {
      char *nextEncStr = strchr(encStr, ' ');
      if (nextEncStr) {
	encStrLen = nextEncStr - encStr;
	nextEncStr++;
      } else {
	encStrLen = strlen(encStr);
      }

      if (strncasecmp(encStr,"raw",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRaw);
      } else if (strncasecmp(encStr,"copyrect",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCopyRect);
      } else if (strncasecmp(encStr,"tight",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingTight);
	requestLastRectEncoding = True;
	if (appData.compressLevel >= 0 && appData.compressLevel <= 9)
	  requestCompressLevel = True;
	if (appData.enableJPEG)
	  requestQualityLevel = True;
      } else if (strncasecmp(encStr,"hextile",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingHextile);
      } else if (strncasecmp(encStr,"zlib",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingZlib);
	if (appData.compressLevel >= 0 && appData.compressLevel <= 9)
	  requestCompressLevel = True;
      } else if (strncasecmp(encStr,"corre",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCoRRE);
      } else if (strncasecmp(encStr,"rre",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRRE);
      } else if (strncasecmp(encStr,"zrle",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingZRLE);
      } else {
	fprintf(stderr,"Unknown encoding '%.*s'\n",encStrLen,encStr);
      }

      encStr = nextEncStr;
    } while (encStr && se->nEncodings < MAX_ENCODINGS);

    if (se->nEncodings < MAX_ENCODINGS && requestCompressLevel) {
      encs[se->nEncodings++] = Swap32IfLE(appData.compressLevel +
					  rfbEncodingCompressLevel0);
    }

    if (se->nEncodings < MAX_ENCODINGS && requestQualityLevel) {
      if (appData.qualityLevel < 0 || appData.qualityLevel > 9)
        appData.qualityLevel = 5;
      encs[se->nEncodings++] = Swap32IfLE(appData.qualityLevel +
					  rfbEncodingQualityLevel0);
    }

      if (se->nEncodings < MAX_ENCODINGS)
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingXCursor);
      if (se->nEncodings < MAX_ENCODINGS)
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRichCursor);
      if (se->nEncodings < MAX_ENCODINGS)
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingPointerPos);

    if (se->nEncodings < MAX_ENCODINGS && requestLastRectEncoding) {
      encs[se->nEncodings++] = Swap32IfLE(rfbEncodingLastRect);
    }
  } else {
    if (sameMachine) {
      if (!tunnelSpecified) {
        if (!appData.quiet) {
	  fprintf(stderr,"Same machine: preferring raw encoding\n");
        }
        currentEncoding = rfbEncodingRaw;
      } else {
        if (!appData.quiet) {
	  fprintf(stderr,"Tunneling active: preferring tight encoding\n");
        }
        currentEncoding = rfbEncodingTight;
      }
    }

    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingLastRect);
    
    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCopyRect);
    encs[se->nEncodings++] = Swap32IfLE(currentEncoding);
    for (i = 0; i < NUM_SUPPORTED_ENCODINGS; i++) {
      if (supportedEncodings[i] != currentEncoding)
        encs[se->nEncodings++] = Swap32IfLE(supportedEncodings[i]);
    }

    if (appData.compressLevel >= 0 && appData.compressLevel <= 9) {
      encs[se->nEncodings++] = Swap32IfLE(appData.compressLevel +
					  rfbEncodingCompressLevel0);
    } else if (!tunnelSpecified) {
      /* If -tunnel option was provided, we assume that server machine is
	 not in the local network so we use default compression level for
	 tight encoding instead of fast compression. Thus we are
	 requesting level 1 compression only if tunneling is not used. */
      encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCompressLevel1);
    }

    if (appData.enableJPEG) {
      if (appData.qualityLevel < 0 || appData.qualityLevel > 9)
	appData.qualityLevel = 5;
      encs[se->nEncodings++] = Swap32IfLE(appData.qualityLevel +
					  rfbEncodingQualityLevel0);
    }

      if (se->nEncodings < MAX_ENCODINGS)
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingXCursor);
      if (se->nEncodings < MAX_ENCODINGS)
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRichCursor);
      if (se->nEncodings < MAX_ENCODINGS)
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingPointerPos);

  }

  len = sz_rfbSetEncodingsMsg + se->nEncodings * 4;

  se->nEncodings = Swap16IfLE(se->nEncodings);

  return WriteToRFBServer(buf, len);
}


/*
 * SendIncrementalFramebufferUpdateRequest.
 */

Bool
SendIncrementalFramebufferUpdateRequest()
{
  return SendFramebufferUpdateRequest(0, 0, si.framebufferWidth,
				      si.framebufferHeight, True);
}

Bool RequestNewUpdate()
{
  if (!SendFramebufferUpdateRequest(appData.rectX, appData.rectY, appData.rectWidth,
                                      appData.rectHeight, True)) {
      return False;
  }

  return True;
}

/*
 * HandleRFBServerMessage.
 */

Bool
HandleRFBServerMessage()
{
  rfbServerToClientMsg msg;

  if (!ReadFromRFBServer((char *)&msg, 1))
    return False;

  switch (msg.type) {

  case rfbSetColourMapEntries:
  {
    fprintf(stderr, "Received unsupported rfbSetColourMapEntries\n");
    return False; /* unsupported */
  }

  case rfbFramebufferUpdate:
  {
    rfbFramebufferUpdateRectHeader rect;
    int linesToRead;
    int bytesPerLine;
    int i;

    if (!ReadFromRFBServer(((char *)&msg.fu) + 1,
			   sz_rfbFramebufferUpdateMsg - 1))
      return False;

    msg.fu.nRects = Swap16IfLE(msg.fu.nRects);

    for (i = 0; i < msg.fu.nRects; i++) {
      if (!ReadFromRFBServer((char *)&rect, sz_rfbFramebufferUpdateRectHeader))
	return False;

      rect.r.x = Swap16IfLE(rect.r.x);
      rect.r.y = Swap16IfLE(rect.r.y);
      rect.r.w = Swap16IfLE(rect.r.w);
      rect.r.h = Swap16IfLE(rect.r.h);

      rect.encoding = Swap32IfLE(rect.encoding);

      if (rect.encoding == rfbEncodingXCursor) {
	if (!HandleCursorShape(rect.r.x, rect.r.y, rect.r.w, rect.r.h, rfbEncodingXCursor)) {
	  return False;
	}
        continue;
      }
      if (rect.encoding == rfbEncodingRichCursor) {
	if (!HandleCursorShape(rect.r.x, rect.r.y, rect.r.w, rect.r.h, rfbEncodingRichCursor)) {
	  return False;
	}
        continue;
      }

      if (rect.encoding == rfbEncodingPointerPos) {
	if (!HandleCursorPos(rect.r.x, rect.r.y)) {
	  return False;
	}
        appData.gotCursorPos = 1;
	continue;
      }

      if ((rect.r.x + rect.r.w > si.framebufferWidth) ||
	  (rect.r.y + rect.r.h > si.framebufferHeight))
	{
	  fprintf(stderr,"Rect too large: %dx%d at (%d, %d)\n",
		  rect.r.w, rect.r.h, rect.r.x, rect.r.y);
	  return False;
	}

      if ((rect.r.h * rect.r.w) == 0) {
	fprintf(stderr,"Zero size rect - ignoring\n");
	continue;
      }

      /* If RichCursor encoding is used, we should prevent collisions
	 between framebuffer updates and cursor drawing operations. */
      SoftCursorLockArea(rect.r.x, rect.r.y, rect.r.w, rect.r.h);

      switch (rect.encoding) {

      case rfbEncodingRaw:

	bytesPerLine = rect.r.w * myFormat.bitsPerPixel / 8;
	linesToRead = BUFFER_SIZE / bytesPerLine;

	while (rect.r.h > 0) {
	  if (linesToRead > rect.r.h)
	    linesToRead = rect.r.h;

	  if (!ReadFromRFBServer(buffer,bytesPerLine * linesToRead))
	    return False;
	  CopyDataToScreen(buffer, rect.r.x, rect.r.y, rect.r.w,
			   linesToRead);

	  rect.r.h -= linesToRead;
	  rect.r.y += linesToRead;

	}
	break;

      case rfbEncodingCopyRect:
      {
	rfbCopyRect cr;
          char *buffer;

	if (!ReadFromRFBServer((char *)&cr, sz_rfbCopyRect))
	  return False;

          if (!BufferWritten()) {
            /* Ignore attempts to do copy-rect when we have nothing to
             * copy from.
             */
            break;
        }

 	cr.srcX = Swap16IfLE(cr.srcX);
	cr.srcY = Swap16IfLE(cr.srcY);

	/* If RichCursor encoding is used, we should extend our
	   "cursor lock area" (previously set to destination
	   rectangle) to the source rectangle as well. */
	SoftCursorLockArea(cr.srcX, cr.srcY, rect.r.w, rect.r.h);

        buffer = CopyScreenToData(cr.srcX, cr.srcY, rect.r.w, rect.r.h);
        CopyDataToScreen(buffer, rect.r.x, rect.r.y, rect.r.w, rect.r.h);
        free(buffer);

	break;
      }

      case rfbEncodingRRE:
      {
	if (!HandleRRE32(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return False;
	break;
      }

      case rfbEncodingCoRRE:
      {
	if (!HandleCoRRE32(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	  return False;
	break;
      }

      case rfbEncodingHextile:
      {
	if (!HandleHextile32(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return False;
	break;
      }

      case rfbEncodingZlib:
      {
	if (!HandleZlib32(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return False;
	break;
     }

      case rfbEncodingTight:
      {
	if (!HandleTight32(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return False;
	break;
      }

      case rfbEncodingZRLE:
        if (!zrleDecode(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
          return False;
	break;

      default:
	fprintf(stderr,"Unknown rect encoding %d\n",
		(int)rect.encoding);
	return False;
      }

      /* Now we may discard "soft cursor locks". */
      SoftCursorUnlockScreen();

        /* Done. Save the screen image. */
    }

      /* RealVNC sometimes returns an initial black screen. */
      if (BufferIsBlank() && appData.ignoreBlank) {
          if (!appData.quiet && appData.ignoreBlank != 1) {
              /* user did not specify either -quiet or -ignoreblank */
              fprintf(stderr, "Warning: discarding received blank screen (use -allowblank to accept,\n   or -ignoreblank to suppress this message)\n");
              appData.ignoreBlank = 1;
          }
          RequestNewUpdate();
      } else {
          return False;
      }

    break;
  }

  case rfbBell:
    /* ignore */
    break;

  case rfbServerCutText:
  {
    if (!ReadFromRFBServer(((char *)&msg) + 1,
			   sz_rfbServerCutTextMsg - 1))
      return False;

    msg.sct.length = Swap32IfLE(msg.sct.length);

    if (serverCutText)
      free(serverCutText);

    serverCutText = malloc(msg.sct.length+1);

    if (!ReadFromRFBServer(serverCutText, msg.sct.length))
      return False;

    serverCutText[msg.sct.length] = 0;

    newServerCutText = True;

    break;
  }

  default:
    fprintf(stderr,"Unknown message type %d from VNC server\n",msg.type);
    return False;
  }

  return True;
}


#define GET_PIXEL8(pix, ptr) ((pix) = *(ptr)++)

#define GET_PIXEL16(pix, ptr) (((CARD8*)&(pix))[0] = *(ptr)++, \
			       ((CARD8*)&(pix))[1] = *(ptr)++)

#ifdef __APPLE__
/* Apple compilation appears to be broken.*/
static inline void GET_PIXEL32(void *pix, CARD8 *ptr)
{
    ((CARD8*)&(pix))[0] = *(ptr)++;
    ((CARD8*)&(pix))[1] = *(ptr)++;
    ((CARD8*)&(pix))[2] = *(ptr)++;
    ((CARD8*)&(pix))[3] = *(ptr)++;
}
#endif
#define GET_PIXEL32(pix, ptr) (((CARD8*)&(pix))[0] = *(ptr)++, \
			       ((CARD8*)&(pix))[1] = *(ptr)++, \
			       ((CARD8*)&(pix))[2] = *(ptr)++, \
			       ((CARD8*)&(pix))[3] = *(ptr)++)

/* CONCAT2 concatenates its two arguments.  CONCAT2E does the same but also
   expands its arguments if they are macros */

#define CONCAT2(a,b) a##b
#define CONCAT2E(a,b) CONCAT2(a,b)

#define BPP 32
#include "protocols/rre.c"
#include "protocols/corre.c"
#include "protocols/hextile.c"
#include "protocols/zlib.c"
#include "protocols/tight.c"
#undef BPP


/*
 * PrintPixelFormat.
 */

void
PrintPixelFormat(format)
    rfbPixelFormat *format;
{
  if (format->bitsPerPixel == 1) {
    fprintf(stderr,"  Single bit per pixel.\n");
    fprintf(stderr,
	    "  %s significant bit in each byte is leftmost on the screen.\n",
	    (format->bigEndian ? "Most" : "Least"));
  } else {
    fprintf(stderr,"  %d bits per pixel.\n",format->bitsPerPixel);
    if (format->bitsPerPixel != 8) {
      fprintf(stderr,"  %s significant byte first in each pixel.\n",
	      (format->bigEndian ? "Most" : "Least"));
    }
    if (format->trueColour) {
      fprintf(stderr,"  True colour: max red %d green %d blue %d",
	      format->redMax, format->greenMax, format->blueMax);
      fprintf(stderr,", shift red %d green %d blue %d\n",
	      format->redShift, format->greenShift, format->blueShift);
    } else {
      fprintf(stderr,"  Colour map (not true colour).\n");
    }
  }
}

static long
ReadCompactLen (void)
{
  long len;
  CARD8 b;

  if (!ReadFromRFBServer((char *)&b, 1))
    return -1;
  len = (int)b & 0x7F;
  if (b & 0x80) {
    if (!ReadFromRFBServer((char *)&b, 1))
      return -1;
    len |= ((int)b & 0x7F) << 7;
    if (b & 0x80) {
      if (!ReadFromRFBServer((char *)&b, 1))
	return -1;
      len |= ((int)b & 0xFF) << 14;
    }
  }
  return len;
}

/*
 * JPEG source manager functions for JPEG decompression in Tight decoder.
 */

static struct jpeg_source_mgr jpegSrcManager;
static JOCTET *jpegBufferPtr;
static size_t jpegBufferLen;

static void
JpegInitSource(j_decompress_ptr cinfo)
{
  jpegError = False;
}

static boolean
JpegFillInputBuffer(j_decompress_ptr cinfo)
{
  jpegError = True;
  jpegSrcManager.bytes_in_buffer = jpegBufferLen;
  jpegSrcManager.next_input_byte = (JOCTET *)jpegBufferPtr;

  return TRUE;
}

static void
JpegSkipInputData(j_decompress_ptr cinfo, long num_bytes)
{
  if (num_bytes < 0 || (unsigned int) num_bytes > jpegSrcManager.bytes_in_buffer) {
    jpegError = True;
    jpegSrcManager.bytes_in_buffer = jpegBufferLen;
    jpegSrcManager.next_input_byte = (JOCTET *)jpegBufferPtr;
  } else {
    jpegSrcManager.next_input_byte += (size_t) num_bytes;
    jpegSrcManager.bytes_in_buffer -= (size_t) num_bytes;
  }
}

static void
JpegTermSource(j_decompress_ptr cinfo)
{
  /* No work necessary here. */
}

static void
JpegSetSrcManager(j_decompress_ptr cinfo, CARD8 *compressedData,
		  int compressedLen)
{
  jpegBufferPtr = (JOCTET *)compressedData;
  jpegBufferLen = (size_t)compressedLen;

  jpegSrcManager.init_source = JpegInitSource;
  jpegSrcManager.fill_input_buffer = JpegFillInputBuffer;
  jpegSrcManager.skip_input_data = JpegSkipInputData;
  jpegSrcManager.resync_to_restart = jpeg_resync_to_restart;
  jpegSrcManager.term_source = JpegTermSource;
  jpegSrcManager.next_input_byte = jpegBufferPtr;
  jpegSrcManager.bytes_in_buffer = jpegBufferLen;

  cinfo->src = &jpegSrcManager;
}

