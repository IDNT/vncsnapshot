//
// Copyright (C) 2002 RealVNC Ltd.  All Rights Reserved.
//
// This is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this software; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
// USA.

//
// zrle.cxx
//
// Decode function for Zlib Run-length Encoding (ZRLE).
//

#include <rdr/ZlibInStream.h>
#include <rdr/FdInStream.h>
#include <rdr/Exception.h>

extern "C" {
#include "vncsnapshot.h"
}

// Instantiate the decoding function for 8, 16 and 32 BPP

#define IMAGE_RECT(x,y,w,h,data)                \
    CopyDataToScreen((char*)data,x,y,w,h);

#define FILL_RECT(x,y,w,h,pix)                                          \
    FillBufferRectangle(x, y, w, h, pix);

#define BPP 8
#include <rfb/zrleDecode.h>
#undef BPP

#undef FILL_RECT
#define FILL_RECT(x,y,w,h,pix)                          \
    FillBufferRectangle(x, y, w, h, pix);

#define BPP 16
#include <rfb/zrleDecode.h>
#undef BPP
#define BPP 32
#include <rfb/zrleDecode.h>
#define CPIXEL 24A
#include <rfb/zrleDecode.h>
#undef CPIXEL
#define CPIXEL 24B
#include <rfb/zrleDecode.h>
#undef CPIXEL
#undef BPP


#define BUFFER_SIZE (rfbZRLETileWidth * rfbZRLETileHeight * 4)
static char buffer[BUFFER_SIZE];

rdr::ZlibInStream zis;
extern rdr::FdInStream* fis;

Bool zrleDecode(int x, int y, int w, int h)
{
  try {
    switch (myFormat.bitsPerPixel) {

    case 8:
      zrleDecode8( x,y,w,h,fis,&zis,(rdr::U8*)buffer);
      break;

    case 16:
      zrleDecode16(x,y,w,h,fis,&zis,(rdr::U16*)buffer);
      break;

    case 32:
      bool fitsInLS3Bytes
        = ((myFormat.redMax   << myFormat.redShift)   < (1<<24) &&
           (myFormat.greenMax << myFormat.greenShift) < (1<<24) &&
           (myFormat.blueMax  << myFormat.blueShift)  < (1<<24));

      bool fitsInMS3Bytes = (myFormat.redShift   > 7  &&
                             myFormat.greenShift > 7  &&
                             myFormat.blueShift  > 7);

      if ((fitsInLS3Bytes && !myFormat.bigEndian) ||
          (fitsInMS3Bytes && myFormat.bigEndian))
      {
        zrleDecode24A(x,y,w,h,fis,&zis,(rdr::U32*)buffer);
      }
      else if ((fitsInLS3Bytes && myFormat.bigEndian) ||
               (fitsInMS3Bytes && !myFormat.bigEndian))
      {
        zrleDecode24B(x,y,w,h,fis,&zis,(rdr::U32*)buffer);
      }
      else
      {
        zrleDecode32(x,y,w,h,fis,&zis,(rdr::U32*)buffer);
      }
      break;
    }

  } catch (rdr::Exception& e) {
    fprintf(stderr,"ZRLE decoder exception: %s\n",e.str());
    return False;
  }

  return True;
}
