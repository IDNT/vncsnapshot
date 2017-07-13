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
 * buffer.c - functions to deal with the raw image buffer.
 */
static const char *ID = "$Id: buffer.c,v 1.6 2004/09/09 00:22:33 grmcdorman Exp $";

#include "vncsnapshot.h"

/* jpeglib.h may redefine INT16 */
#define INT16 jpegINT16
#include <jpeglib.h>
#include <stdio.h>
#undef INT16

static void BufferPixelToRGB(unsigned long pixel, int *r, int *g, int *b);

static char * rawBuffer = NULL;
static char   bufferBlank = 1;
static char   bufferWritten = 0;

#define RAW_BYTES_PER_PIXEL 3   /* size of pixel in raw buffer */
#define MY_BYTES_PER_PIXEL 4    /* size of pixel in VNC buffer */
#define MY_BITS_PER_PIXEL (MY_BYTES_PER_PIXEL*8)

int
AllocateBuffer()
{
    unsigned long bytes;
    static const short testEndian = 1;
    int bigEndian;

    /* Determine 'endian' nature of this machine */
    /* On big-endian machines, the address of a short (16 bit) is the
     * most significant byte (and is therefore 0). On little-endian,
     * it is the address of the least significant byte - and is therefore
     * 1.
     *
     * Intel 8x86 (including Pentium) are big-endian. Motorola, PDP-11,
     * and Sparc are little-endian.
     */
    bigEndian = 0 == *(char *)&testEndian;
//bigEndian = 0;
    /* Format is RGBA. Due to the way we store the pixels,
     * the 'bigEndian' is the *opposite* of the hardware value.
     */
    myFormat.bitsPerPixel = MY_BITS_PER_PIXEL;
    myFormat.depth = 24;
    myFormat.trueColour = 1;
    myFormat.bigEndian = bigEndian;
    if (bigEndian) {
        myFormat.redShift = 24;
        myFormat.greenShift = 16;
        myFormat.blueShift = 8;
    } else {
        myFormat.redShift = 0;
        myFormat.greenShift = 8;
        myFormat.blueShift = 16;
    }
    myFormat.redMax = 0xFF;
    myFormat.greenMax = 0xFF;
    myFormat.blueMax = 0xFF;

    bytes = si.framebufferWidth * si.framebufferHeight * myFormat.depth / 8;
    rawBuffer = malloc(bytes);   /* allocate initialized to 0 */
    if (rawBuffer == NULL) {
        fprintf(stderr, "Failed to allocate memory frame buffer, %lu bytes\n",
                bytes);
        return 0;
    }

    memset(rawBuffer, 0xBA, bytes);

    return 1;
}

void
CopyDataToScreen(char *buffer, int x, int y, int w, int h)
{
    int start;
    int stride;
    int row, col;
    stride = si.framebufferWidth * RAW_BYTES_PER_PIXEL - w * RAW_BYTES_PER_PIXEL;
    start = (x + y * si.framebufferWidth) * RAW_BYTES_PER_PIXEL;

    bufferWritten = 1;

    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            bufferBlank &= buffer[0] == 0 &&
                           buffer[1] == 0 &&
                           buffer[2] == 0;
            rawBuffer[start++] = *buffer++;
            rawBuffer[start++] = *buffer++;
            rawBuffer[start++] = *buffer++;
            buffer++;   /* ignore 4th byte */
        }
        start += stride;
    }
}

char *
CopyScreenToData(int x, int y, int w, int h)
{
    int start;
    int stride;
    int row, col;
    char *buffer;
    char *cp;

    stride = si.framebufferWidth * RAW_BYTES_PER_PIXEL - w * RAW_BYTES_PER_PIXEL;
    start = (x + y * si.framebufferWidth) * RAW_BYTES_PER_PIXEL;


    /* Allocate a buffer at the VNC size, not the raw size */
    buffer = malloc(h * w * MY_BYTES_PER_PIXEL);
    cp = buffer;

    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            *cp++ = rawBuffer[start++];
            *cp++ = rawBuffer[start++];
            *cp++ = rawBuffer[start++];
            *cp++ = 0;
        }
        start += stride;
    }

    return buffer;
}

void
FillBufferRectangle(int x, int y, int w, int h, unsigned long pixel)
{
    int r, g, b;
    int start;
    int stride;
    int row, col;

    BufferPixelToRGB(pixel, &r, &g, &b);

    bufferBlank &= r == 0 && g == 0 && b == 0;
    bufferWritten = 1;

    stride = si.framebufferWidth * RAW_BYTES_PER_PIXEL - w * RAW_BYTES_PER_PIXEL;
    start = (x + y * si.framebufferWidth) * RAW_BYTES_PER_PIXEL;
    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            rawBuffer[start++] = r;
            rawBuffer[start++] = g;
            rawBuffer[start++] = b;
        }
        start += stride;
    }
}

int
BufferIsBlank()
{
    return bufferBlank;
}

int
BufferWritten()
{
    return bufferWritten;
}

/*
 * Borrowed with very minor modifications from JPEG6 sample code. Error handling
 * remains as default (i.e. exit on errors).
 */
void
write_JPEG_file (char * filename, int quality, int width, int height)
{
  /* This struct contains the JPEG compression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   * It is possible to have several such structures, representing multiple
   * compression/decompression processes, in existence at once.  We refer
   * to any one struct (and its associated working data) as a "JPEG object".
   */
  struct jpeg_compress_struct cinfo;
  /* This struct represents a JPEG error handler.  It is declared separately
   * because applications often want to supply a specialized error handler
   * (see the second half of this file for an example).  But here we just
   * take the easy way out and use the standard error handler, which will
   * print a message on stderr and call exit() if compression fails.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  struct jpeg_error_mgr jerr;
  /* More stuff */
  FILE * outfile;		/* target file */
  JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
  int row_stride;		/* physical row width in image buffer */

  /* Step 1: allocate and initialize JPEG compression object */

  /* We have to set up the error handler first, in case the initialization
   * step fails.  (Unlikely, but it could happen if you are out of memory.)
   * This routine fills in the contents of struct jerr, and returns jerr's
   * address which we place into the link field in cinfo.
   */
  cinfo.err = jpeg_std_error(&jerr);
  /* Now we can initialize the JPEG compression object. */
  jpeg_create_compress(&cinfo);

  /* Step 2: specify data destination (eg, a file) */
  /* Note: steps 2 and 3 can be done in either order. */

  /* Here we use the library-supplied code to send compressed data to a
   * stdio stream.  You can also write your own code to do something else.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to write binary files.
   */
  if (strcmp(filename, "-") == 0) {
      outfile = stdout;
  } else {
      if ((outfile = fopen(filename, "wb")) == NULL) {
          fprintf(stderr, "can't open %s\n", filename);
          exit(1);
      }
  }
  jpeg_stdio_dest(&cinfo, outfile);

  /* Step 3: set parameters for compression */

  /* First we supply a description of the input image.
   * Four fields of the cinfo struct must be filled in:
   */
  cinfo.image_width = width; 	/* image width and height, in pixels */
  cinfo.image_height = height;
  cinfo.input_components = RAW_BYTES_PER_PIXEL;		/* # of color components per pixel */
  cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */
  /* Now use the library's routine to set default compression parameters.
   * (You must set at least cinfo.in_color_space before calling this,
   * since the defaults depend on the source color space.)
   */
  jpeg_set_defaults(&cinfo);
  /* Now you can set any non-default parameters you wish to.
   * Here we just illustrate the use of quality (quantization table) scaling:
   */

  jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

    /* VNCSNAPSHOT: Set file colourspace to RGB.
     *   If it is not set to RGB, colour distortions occur.
     */
  jpeg_set_colorspace(&cinfo, JCS_RGB);

  /* Step 4: Start compressor */

  /* TRUE ensures that we will write a complete interchange-JPEG file.
   * Pass TRUE unless you are very sure of what you're doing.
   */
  jpeg_start_compress(&cinfo, TRUE);

  /* Step 5: while (scan lines remain to be written) */
  /*           jpeg_write_scanlines(...); */

  /* Here we use the library's state variable cinfo.next_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   * To keep things simple, we pass one scanline per call; you can pass
   * more if you wish, though.
   */
  row_stride = width * 3;	/* JSAMPLEs per row in image_buffer */

  while (cinfo.next_scanline < cinfo.image_height) {
    /* jpeg_write_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could pass
     * more than one scanline at a time if that's more convenient.
     */
    row_pointer[0] = & rawBuffer[cinfo.next_scanline * row_stride];
    (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  /* Step 6: Finish compression */

  jpeg_finish_compress(&cinfo);
  /* After finish_compress, we can close the output file. */
  if (strcmp(filename, "-") != 0) {
      fclose(outfile);
  }

  /* Step 7: release JPEG compression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_compress(&cinfo);

  /* And we're done! */
}

static void
BufferPixelToRGB(unsigned long pixel, int *r, int *g, int *b)
{
    *r = (pixel >> myFormat.redShift) & myFormat.redMax;
    *b = (pixel >> myFormat.blueShift) & myFormat.blueMax;
    *g = (pixel >> myFormat.greenShift) & myFormat.greenMax;
}

void
ShrinkBuffer(long x, long y, long req_width, long req_height)
{
    int start;
    int stride;
    int row, col;
    char *cp;


    /*
     * Don't bother if x and y are zero and the width is the same.
     */
    if (x == 0 && y == 0 && req_width == si.framebufferWidth) {
        return;
    }

    /*
     * Rather than creating a copy, we just move in-place. Since we are
     * doing this from the start of the image, there is no problem
     * with overlapping moves.
     */

    stride = si.framebufferWidth * RAW_BYTES_PER_PIXEL - req_width * RAW_BYTES_PER_PIXEL;
    start = (x + y * si.framebufferWidth) * RAW_BYTES_PER_PIXEL;


    cp = rawBuffer;

    for (row = 0; row < req_height; row++) {
        for (col = 0; col < req_width; col++) {
            *cp++ = rawBuffer[start++];
            *cp++ = rawBuffer[start++];
            *cp++ = rawBuffer[start++];
        }
        start += stride;
    }
    
}
  
