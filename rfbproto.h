/*
 *  Copyright (C) 2002 Ultr@VNC Team Members. All Rights Reserved.
 *  Copyright (C) 2002 RealVNC Ltd.  All Rights Reserved.
 *  Copyright (C) 2000-2002 Constantin Kaplinsky.  All Rights Reserved.
 *  Copyright (C) 2000 Tridia Corporation. All Rights Reserved.
 *  Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
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
 * rfbproto.h - header file for the RFB protocol version 3.3
 *
 * Uses types CARD<n> for an n-bit unsigned integer, INT<n> for an n-bit signed
 * integer (for n = 8, 16 and 32).
 *
 * All multiple byte integers are in big endian (network) order (most
 * significant byte first).  Unless noted otherwise there is no special
 * alignment of protocol structures.
 *
 *
 * Once the initial handshaking is done, all messages start with a type byte,
 * (usually) followed by message-specific data.  The order of definitions in
 * this file is as follows:
 *
 *  (1) Structures used in several types of message.
 *  (2) Structures used in the initial handshaking.
 *  (3) Message types.
 *  (4) Encoding types.
 *  (5) For each message type, the form of the data following the type byte.
 *      Sometimes this is defined by a single structure but the more complex
 *      messages have to be explained by comments.
 */


/*****************************************************************************
 *
 * Structures used in several messages
 *
 *****************************************************************************/

/*-----------------------------------------------------------------------------
 * Structure used to specify a rectangle.  This structure is a multiple of 4
 * bytes so that it can be interspersed with 32-bit pixel data without
 * affecting alignment.
 */

typedef struct {
    CARD16 x;
    CARD16 y;
    CARD16 w;
    CARD16 h;
} rfbRectangle;

#define sz_rfbRectangle 8


/*-----------------------------------------------------------------------------
 * Structure used to specify pixel format.
 */

typedef struct {

    CARD8 bitsPerPixel;     /* 8,16,32 only */

    CARD8 depth;        /* 8 to 32 */

    CARD8 bigEndian;        /* True if multi-byte pixels are interpreted
                   as big endian, or if single-bit-per-pixel
                   has most significant bit of the byte
                   corresponding to first (leftmost) pixel. Of
                   course this is meaningless for 8 bits/pix */

    CARD8 trueColour;       /* If false then we need a "colour map" to
                   convert pixels to RGB.  If true, xxxMax and
                   xxxShift specify bits used for red, green
                   and blue */

    /* the following fields are only meaningful if trueColour is true */

    CARD16 redMax;      /* maximum red value (= 2^n - 1 where n is the
                   number of bits used for red). Note this
                   value is always in big endian order. */

    CARD16 greenMax;        /* similar for green */

    CARD16 blueMax;     /* and blue */

    CARD8 redShift;     /* number of shifts needed to get the red
                   value in a pixel to the least significant
                   bit. To find the red value from a given
                   pixel, do the following:
                   1) Swap pixel value according to bigEndian
                      (e.g. if bigEndian is false and host byte
                      order is big endian, then swap).
                   2) Shift right by redShift.
                   3) AND with redMax (in host byte order).
                   4) You now have the red value between 0 and
                      redMax. */

    CARD8 greenShift;       /* similar for green */

    CARD8 blueShift;        /* and blue */

    CARD8 pad1;
    CARD16 pad2;

} rfbPixelFormat;

#define sz_rfbPixelFormat 16



/*****************************************************************************
 *
 * Initial handshaking messages
 *
 *****************************************************************************/

/*-----------------------------------------------------------------------------
 * Protocol Version
 *
 * The server always sends 12 bytes to start which identifies the latest RFB
 * protocol version number which it supports.  These bytes are interpreted
 * as a string of 12 ASCII characters in the format "RFB xxx.yyy\n" where
 * xxx and yyy are the major and minor version numbers (for version 3.3
 * this is "RFB 003.003\n").
 *
 * The client then replies with a similar 12-byte message giving the version
 * number of the protocol which should actually be used (which may be different
 * to that quoted by the server).
 *
 * It is intended that both clients and servers may provide some level of
 * backwards compatibility by this mechanism.  Servers in particular should
 * attempt to provide backwards compatibility, and even forwards compatibility
 * to some extent.  For example if a client demands version 3.1 of the
 * protocol, a 3.0 server can probably assume that by ignoring requests for
 * encoding types it doesn't understand, everything will still work OK.  This
 * will probably not be the case for changes in the major version number.
 *
 * The format string below can be used in sprintf or sscanf to generate or
 * decode the version string respectively.
 */

#define rfbProtocolVersionFormat "RFB %03d.%03d\n"
#define rfbProtocolMajorVersion 3
#define rfbProtocolMinorVersion 3
/* Other protocol numbers:
 *    4     UltraVNC
 *    6     UltraVNC
 */

typedef char rfbProtocolVersionMsg[13]; /* allow extra byte for null */

#define sz_rfbProtocolVersionMsg 12


/*-----------------------------------------------------------------------------
 * Authentication
 *
 * Once the protocol version has been decided, the server then sends a 32-bit
 * word indicating whether any authentication is needed on the connection.
 * The value of this word determines the authentication scheme in use.  For
 * version 3.0 of the protocol this may have one of the following values:
 */

#define rfbConnFailed 0
#define rfbNoAuth 1
#define rfbVncAuth 2

/*
 * rfbConnFailed:   For some reason the connection failed (e.g. the server
 *          cannot support the desired protocol version).  This is
 *          followed by a string describing the reason (where a
 *          string is specified as a 32-bit length followed by that
 *          many ASCII characters).
 *
 * rfbNoAuth:       No authentication is needed.
 *
 * rfbVncAuth:      The VNC authentication scheme is to be used.  A 16-byte
 *          challenge follows, which the client encrypts as
 *          appropriate using the password and sends the resulting
 *          16-byte response.  If the response is correct, the
 *          server sends the 32-bit word rfbVncAuthOK.  If a simple
 *          failure happens, the server sends rfbVncAuthFailed and
 *          closes the connection. If the server decides that too
 *          many failures have occurred, it sends rfbVncAuthTooMany
 *          and closes the connection.  In the latter case, the
 *          server should not allow an immediate reconnection by
 *          the client.
 */

#define rfbVncAuthOK 0
#define rfbVncAuthFailed 1
#define rfbVncAuthTooMany 2


/*-----------------------------------------------------------------------------
 * Client Initialisation Message
 *
 * Once the client and server are sure that they're happy to talk to one
 * another, the client sends an initialisation message.  At present this
 * message only consists of a boolean indicating whether the server should try
 * to share the desktop by leaving other clients connected, or give exclusive
 * access to this client by disconnecting all other clients.
 */

typedef struct {
    CARD8 shared;
} rfbClientInitMsg;

#define sz_rfbClientInitMsg 1


/*-----------------------------------------------------------------------------
 * Server Initialisation Message
 *
 * After the client initialisation message, the server sends one of its own.
 * This tells the client the width and height of the server's framebuffer,
 * its pixel format and the name associated with the desktop.
 */

typedef struct {
    CARD16 framebufferWidth;
    CARD16 framebufferHeight;
    rfbPixelFormat format;  /* the server's preferred pixel format */
    CARD32 nameLength;
    /* followed by char name[nameLength] */
} rfbServerInitMsg;

#define sz_rfbServerInitMsg (8 + sz_rfbPixelFormat)


/*
 * Following the server initialisation message it's up to the client to send
 * whichever protocol messages it wants.  Typically it will send a
 * SetPixelFormat message and a SetEncodings message, followed by a
 * FramebufferUpdateRequest.  From then on the server will send
 * FramebufferUpdate messages in response to the client's
 * FramebufferUpdateRequest messages.  The client should send
 * FramebufferUpdateRequest messages with incremental set to true when it has
 * finished processing one FramebufferUpdate and is ready to process another.
 * With a fast client, the rate at which FramebufferUpdateRequests are sent
 * should be regulated to avoid hogging the network.
 */



/*****************************************************************************
 *
 * Message types
 *
 *****************************************************************************/

/* server -> client */

#define rfbFramebufferUpdate 0
#define rfbSetColourMapEntries 1
#define rfbBell 2
#define rfbServerCutText 3

/* UltraVNC/PalmVNC messages */
#define rfbResizeFrameBuffer 4 /* Modif sf@2002 */
#define rfbPalmVNCReSizeFrameBuffer 0xF


/* client -> server */

#define rfbSetPixelFormat 0
#define rfbFixColourMapEntries 1    /* not currently supported */
#define rfbSetEncodings 2
#define rfbFramebufferUpdateRequest 3
#define rfbKeyEvent 4
#define rfbPointerEvent 5
#define rfbClientCutText 6

/* UltraVNC/PalmVNC messages */
#define rfbFileTransfer     7     /* Modif sf@2002 - actually bidirectionnal */
#define rfbSetScale         8     /* Modif sf@2002 */
#define rfbSetServerInput   9     /* Modif rdv@2002 */
#define rfbSetSW       10     /* Modif rdv@2002 */
#define rfbTextChat        11     /* Modif sf@2002 - TextChat - Bidirectionnal */
#define rfbPalmVNCSetScaleFactor 0xF /* PalmVNC 1.4 & 2.0 SetScale Factor message */

#define rfbEnableExtensionRequest 10
#define rfbExtensionData 11

/*****************************************************************************
 *
 * Encoding types
 *
 *****************************************************************************/

#define rfbEncodingRaw       0
#define rfbEncodingCopyRect  1
#define rfbEncodingRRE       2
#define rfbEncodingCoRRE     4
#define rfbEncodingHextile   5
#define rfbEncodingZlib      6
#define rfbEncodingTight     7
#define rfbEncodingZlibHex   8
#define rfbEncodingZRLE 16

/*
 * Special encoding numbers:
 *   0xFFFFFF00 .. 0xFFFFFF0F -- encoding-specific compression levels;
 *   0xFFFFFF10 .. 0xFFFFFF1F -- mouse cursor shape data;
 *   0xFFFFFF20 .. 0xFFFFFF2F -- various protocol extensions;
 *   0xFFFFFF30 .. 0xFFFFFFDF -- not allocated yet;
 *   0xFFFFFFE0 .. 0xFFFFFFEF -- quality level for JPEG compressor;
 *   0xFFFFFFF0 .. 0xFFFFFFFF -- cross-encoding compression levels.
 */

/* UltraVNC */
/* Cache & XOR-Zlib - rdv@2002 */
#define rfbEncodingCache                0xFFFF0000
#define rfbEncodingCacheEnable          0xFFFF0001
#define rfbEncodingXOR_Zlib             0xFFFF0002
#define rfbEncodingXORMonoColor_Zlib    0xFFFF0003
#define rfbEncodingXORMultiColor_Zlib   0xFFFF0004
#define rfbEncodingSolidColor           0xFFFF0005
#define rfbEncodingXOREnable            0xFFFF0006
#define rfbEncodingCacheZip             0xFFFF0007
#define rfbEncodingSolMonoZip           0xFFFF0008

#define rfbEncodingCompressLevel0  0xFFFFFF00
#define rfbEncodingCompressLevel1  0xFFFFFF01
#define rfbEncodingCompressLevel2  0xFFFFFF02
#define rfbEncodingCompressLevel3  0xFFFFFF03
#define rfbEncodingCompressLevel4  0xFFFFFF04
#define rfbEncodingCompressLevel5  0xFFFFFF05
#define rfbEncodingCompressLevel6  0xFFFFFF06
#define rfbEncodingCompressLevel7  0xFFFFFF07
#define rfbEncodingCompressLevel8  0xFFFFFF08
#define rfbEncodingCompressLevel9  0xFFFFFF09

#define rfbEncodingXCursor         0xFFFFFF10
#define rfbEncodingRichCursor      0xFFFFFF11
#define rfbEncodingPointerPos      0xFFFFFF18

#define rfbEncodingLastRect        0xFFFFFF20
#define rfbEncodingNewFBSize       0xFFFFFF21   /* UltraVNC */

#define rfbEncodingQualityLevel0   0xFFFFFFE0
#define rfbEncodingQualityLevel1   0xFFFFFFE1
#define rfbEncodingQualityLevel2   0xFFFFFFE2
#define rfbEncodingQualityLevel3   0xFFFFFFE3
#define rfbEncodingQualityLevel4   0xFFFFFFE4
#define rfbEncodingQualityLevel5   0xFFFFFFE5
#define rfbEncodingQualityLevel6   0xFFFFFFE6
#define rfbEncodingQualityLevel7   0xFFFFFFE7
#define rfbEncodingQualityLevel8   0xFFFFFFE8
#define rfbEncodingQualityLevel9   0xFFFFFFE9


/*****************************************************************************
 *
 * Server -> client message definitions
 *
 *****************************************************************************/


/*-----------------------------------------------------------------------------
 * FramebufferUpdate - a block of rectangles to be copied to the framebuffer.
 *
 * This message consists of a header giving the number of rectangles of pixel
 * data followed by the rectangles themselves.  The header is padded so that
 * together with the type byte it is an exact multiple of 4 bytes (to help
 * with alignment of 32-bit pixels):
 */

typedef struct {
    CARD8 type;         /* always rfbFramebufferUpdate */
    CARD8 pad;
    CARD16 nRects;
    /* followed by nRects rectangles */
} rfbFramebufferUpdateMsg;

#define sz_rfbFramebufferUpdateMsg 4

/*
 * Each rectangle of pixel data consists of a header describing the position
 * and size of the rectangle and a type word describing the encoding of the
 * pixel data, followed finally by the pixel data.  Note that if the client has
 * not sent a SetEncodings message then it will only receive raw pixel data.
 * Also note again that this structure is a multiple of 4 bytes.
 */

typedef struct {
    rfbRectangle r;
    CARD32 encoding;    /* one of the encoding types rfbEncoding... */
} rfbFramebufferUpdateRectHeader;

#define sz_rfbFramebufferUpdateRectHeader (sz_rfbRectangle + 4)


/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Raw Encoding.  Pixels are sent in top-to-bottom scanline order,
 * left-to-right within a scanline with no padding in between.
 */


/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * CopyRect Encoding.  The pixels are specified simply by the x and y position
 * of the source rectangle.
 */

typedef struct {
    CARD16 srcX;
    CARD16 srcY;
} rfbCopyRect;

#define sz_rfbCopyRect 4


/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * RRE - Rise-and-Run-length Encoding.  We have an rfbRREHeader structure
 * giving the number of subrectangles following.  Finally the data follows in
 * the form [<bgpixel><subrect><subrect>...] where each <subrect> is
 * [<pixel><rfbRectangle>].
 */

typedef struct {
    CARD32 nSubrects;
} rfbRREHeader;

#define sz_rfbRREHeader 4


/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * CoRRE - Compact RRE Encoding.  We have an rfbRREHeader structure giving
 * the number of subrectangles following.  Finally the data follows in the form
 * [<bgpixel><subrect><subrect>...] where each <subrect> is
 * [<pixel><rfbCoRRERectangle>].  This means that
 * the whole rectangle must be at most 255x255 pixels.
 */

typedef struct {
    CARD8 x;
    CARD8 y;
    CARD8 w;
    CARD8 h;
} rfbCoRRERectangle;

#define sz_rfbCoRRERectangle 4


/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Hextile Encoding.  The rectangle is divided up into "tiles" of 16x16 pixels,
 * starting at the top left going in left-to-right, top-to-bottom order.  If
 * the width of the rectangle is not an exact multiple of 16 then the width of
 * the last tile in each row will be correspondingly smaller.  Similarly if the
 * height is not an exact multiple of 16 then the height of each tile in the
 * final row will also be smaller.  Each tile begins with a "subencoding" type
 * byte, which is a mask made up of a number of bits.  If the Raw bit is set
 * then the other bits are irrelevant; w*h pixel values follow (where w and h
 * are the width and height of the tile).  Otherwise the tile is encoded in a
 * similar way to RRE, except that the position and size of each subrectangle
 * can be specified in just two bytes.  The other bits in the mask are as
 * follows:
 *
 * BackgroundSpecified - if set, a pixel value follows which specifies
 *    the background colour for this tile.  The first non-raw tile in a
 *    rectangle must have this bit set.  If this bit isn't set then the
 *    background is the same as the last tile.
 *
 * ForegroundSpecified - if set, a pixel value follows which specifies
 *    the foreground colour to be used for all subrectangles in this tile.
 *    If this bit is set then the SubrectsColoured bit must be zero.
 *
 * AnySubrects - if set, a single byte follows giving the number of
 *    subrectangles following.  If not set, there are no subrectangles (i.e.
 *    the whole tile is just solid background colour).
 *
 * SubrectsColoured - if set then each subrectangle is preceded by a pixel
 *    value giving the colour of that subrectangle.  If not set, all
 *    subrectangles are the same colour, the foreground colour;  if the
 *    ForegroundSpecified bit wasn't set then the foreground is the same as
 *    the last tile.
 *
 * The position and size of each subrectangle is specified in two bytes.  The
 * Pack macros below can be used to generate the two bytes from x, y, w, h,
 * and the Extract macros can be used to extract the x, y, w, h values from
 * the two bytes.
 */

#define rfbHextileRaw           (1 << 0)
#define rfbHextileBackgroundSpecified   (1 << 1)
#define rfbHextileForegroundSpecified   (1 << 2)
#define rfbHextileAnySubrects       (1 << 3)
#define rfbHextileSubrectsColoured  (1 << 4)

#define rfbHextilePackXY(x,y) (((x) << 4) | (y))
#define rfbHextilePackWH(w,h) ((((w)-1) << 4) | ((h)-1))
#define rfbHextileExtractX(byte) ((byte) >> 4)
#define rfbHextileExtractY(byte) ((byte) & 0xf)
#define rfbHextileExtractW(byte) (((byte) >> 4) + 1)
#define rfbHextileExtractH(byte) (((byte) & 0xf) + 1)


/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * zlib - zlib compressed Encoding.  We have an rfbZlibHeader structure
 * giving the number of bytes following.  Finally the data follows is
 * zlib compressed version of the raw pixel data as negotiated.
 */

typedef struct {
    CARD32 nBytes;
} rfbZlibHeader;

#define sz_rfbZlibHeader 4

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * ZRLE - encoding combining Zlib compression, tiling, palettisation and
 * run-length encoding.
 */

typedef struct {
    CARD32 length;
} rfbZRLEHeader;

#define sz_rfbZRLEHeader 4

#define rfbZRLETileWidth 64
#define rfbZRLETileHeight 64

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Tight Encoding.
 *
 *-- The first byte of each Tight-encoded rectangle is a "compression control
 *   byte". Its format is as follows (bit 0 is the least significant one):
 *
 *   bit 0:    if 1, then compression stream 0 should be reset;
 *   bit 1:    if 1, then compression stream 1 should be reset;
 *   bit 2:    if 1, then compression stream 2 should be reset;
 *   bit 3:    if 1, then compression stream 3 should be reset;
 *   bits 7-4: if 1000 (0x08), then the compression type is "fill",
 *             if 1001 (0x09), then the compression type is "jpeg",
 *             if 0xxx, then the compression type is "basic",
 *             values greater than 1001 are not valid.
 *
 * If the compression type is "basic", then bits 6..4 of the
 * compression control byte (those xxx in 0xxx) specify the following:
 *
 *   bits 5-4:  decimal representation is the index of a particular zlib
 *              stream which should be used for decompressing the data;
 *   bit 6:     if 1, then a "filter id" byte is following this byte.
 *
 *-- The data that follows after the compression control byte described
 * above depends on the compression type ("fill", "jpeg" or "basic").
 *
 *-- If the compression type is "fill", then the only pixel value follows, in
 * client pixel format (see NOTE 1). This value applies to all pixels of the
 * rectangle.
 *
 *-- If the compression type is "jpeg", the following data stream looks like
 * this:
 *
 *   1..3 bytes:  data size (N) in compact representation;
 *   N bytes:     JPEG image.
 *
 * Data size is compactly represented in one, two or three bytes, according
 * to the following scheme:
 *
 *  0xxxxxxx                    (for values 0..127)
 *  1xxxxxxx 0yyyyyyy           (for values 128..16383)
 *  1xxxxxxx 1yyyyyyy zzzzzzzz  (for values 16384..4194303)
 *
 * Here each character denotes one bit, xxxxxxx are the least significant 7
 * bits of the value (bits 0-6), yyyyyyy are bits 7-13, and zzzzzzzz are the
 * most significant 8 bits (bits 14-21). For example, decimal value 10000
 * should be represented as two bytes: binary 10010000 01001110, or
 * hexadecimal 90 4E.
 *
 *-- If the compression type is "basic" and bit 6 of the compression control
 * byte was set to 1, then the next (second) byte specifies "filter id" which
 * tells the decoder what filter type was used by the encoder to pre-process
 * pixel data before the compression. The "filter id" byte can be one of the
 * following:
 *
 *   0:  no filter ("copy" filter);
 *   1:  "palette" filter;
 *   2:  "gradient" filter.
 *
 *-- If bit 6 of the compression control byte is set to 0 (no "filter id"
 * byte), or if the filter id is 0, then raw pixel values in the client
 * format (see NOTE 1) will be compressed. See below details on the
 * compression.
 *
 *-- The "gradient" filter pre-processes pixel data with a simple algorithm
 * which converts each color component to a difference between a "predicted"
 * intensity and the actual intensity. Such a technique does not affect
 * uncompressed data size, but helps to compress photo-like images better. 
 * Pseudo-code for converting intensities to differences is the following:
 *
 *   P[i,j] := V[i-1,j] + V[i,j-1] - V[i-1,j-1];
 *   if (P[i,j] < 0) then P[i,j] := 0;
 *   if (P[i,j] > MAX) then P[i,j] := MAX;
 *   D[i,j] := V[i,j] - P[i,j];
 *
 * Here V[i,j] is the intensity of a color component for a pixel at
 * coordinates (i,j). MAX is the maximum value of intensity for a color
 * component.
 *
 *-- The "palette" filter converts true-color pixel data to indexed colors
 * and a palette which can consist of 2..256 colors. If the number of colors
 * is 2, then each pixel is encoded in 1 bit, otherwise 8 bits is used to
 * encode one pixel. 1-bit encoding is performed such way that the most
 * significant bits correspond to the leftmost pixels, and each raw of pixels
 * is aligned to the byte boundary. When "palette" filter is used, the
 * palette is sent before the pixel data. The palette begins with an unsigned
 * byte which value is the number of colors in the palette minus 1 (i.e. 1
 * means 2 colors, 255 means 256 colors in the palette). Then follows the
 * palette itself which consist of pixel values in client pixel format (see
 * NOTE 1).
 *
 *-- The pixel data is compressed using the zlib library. But if the data
 * size after applying the filter but before the compression is less then 12,
 * then the data is sent as is, uncompressed. Four separate zlib streams
 * (0..3) can be used and the decoder should read the actual stream id from
 * the compression control byte (see NOTE 2).
 *
 * If the compression is not used, then the pixel data is sent as is,
 * otherwise the data stream looks like this:
 *
 *   1..3 bytes:  data size (N) in compact representation;
 *   N bytes:     zlib-compressed data.
 *
 * Data size is compactly represented in one, two or three bytes, just like
 * in the "jpeg" compression method (see above).
 *
 *-- NOTE 1. If the color depth is 24, and all three color components are
 * 8-bit wide, then one pixel in Tight encoding is always represented by
 * three bytes, where the first byte is red component, the second byte is
 * green component, and the third byte is blue component of the pixel color
 * value. This applies to colors in palettes as well.
 *
 *-- NOTE 2. The decoder must reset compression streams' states before
 * decoding the rectangle, if some of bits 0,1,2,3 in the compression control
 * byte are set to 1. Note that the decoder must reset zlib streams even if
 * the compression type is "fill" or "jpeg".
 *
 *-- NOTE 3. The "gradient" filter and "jpeg" compression may be used only
 * when bits-per-pixel value is either 16 or 32, not 8.
 *
 *-- NOTE 4. The width of any Tight-encoded rectangle cannot exceed 2048
 * pixels. If a rectangle is wider, it must be split into several rectangles
 * and each one should be encoded separately.
 *
 */

#define rfbTightExplicitFilter         0x04
#define rfbTightFill                   0x08
#define rfbTightJpeg                   0x09
#define rfbTightMaxSubencoding         0x09

/* Filters to improve compression efficiency */
#define rfbTightFilterCopy             0x00
#define rfbTightFilterPalette          0x01
#define rfbTightFilterGradient         0x02


/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * ZLIBHEX - zlib compressed Hextile Encoding.  Essentially, this is the
 * hextile encoding with zlib compression on the tiles that can not be
 * efficiently encoded with one of the other hextile subencodings.  The
 * new zlib subencoding uses two bytes to specify the length of the
 * compressed tile and then the compressed data follows.  As with the
 * raw sub-encoding, the zlib subencoding invalidates the other
 * values, if they are also set.
 */

#define rfbHextileZlibRaw       (1 << 5)
#define rfbHextileZlibHex       (1 << 6)
#define rfbHextileZlibMono      (1 << 7)


/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * XCursor encoding. This is a special encoding used to transmit X-style
 * cursor shapes from server to clients. Note that for this encoding,
 * coordinates in rfbFramebufferUpdateRectHeader structure hold hotspot
 * position (r.x, r.y) and cursor size (r.w, r.h). If (w * h != 0), two RGB
 * samples are sent after header in the rfbXCursorColors structure. They
 * denote foreground and background colors of the cursor. If a client
 * supports only black-and-white cursors, it should ignore these colors and
 * assume that foreground is black and background is white. Next, two bitmaps
 * (1 bits per pixel) follow: first one with actual data (value 0 denotes
 * background color, value 1 denotes foreground color), second one with
 * transparency data (bits with zero value mean that these pixels are
 * transparent). Both bitmaps represent cursor data in a byte stream, from
 * left to right, from top to bottom, and each row is byte-aligned. Most
 * significant bits correspond to leftmost pixels. The number of bytes in
 * each row can be calculated as ((w + 7) / 8). If (w * h == 0), cursor
 * should be hidden (or default local cursor should be set by the client).
 */

typedef struct {
    CARD8 foreRed;
    CARD8 foreGreen;
    CARD8 foreBlue;
    CARD8 backRed;
    CARD8 backGreen;
    CARD8 backBlue;
} rfbXCursorColors;

#define sz_rfbXCursorColors 6


/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * RichCursor encoding. This is a special encoding used to transmit cursor
 * shapes from server to clients. It is similar to the XCursor encoding but
 * uses client pixel format instead of two RGB colors to represent cursor
 * image. For this encoding, coordinates in rfbFramebufferUpdateRectHeader
 * structure hold hotspot position (r.x, r.y) and cursor size (r.w, r.h).
 * After header, two pixmaps follow: first one with cursor image in current
 * client pixel format (like in raw encoding), second with transparency data
 * (1 bit per pixel, exactly the same format as used for transparency bitmap
 * in the XCursor encoding). If (w * h == 0), cursor should be hidden (or
 * default local cursor should be set by the client).
 */


/*-----------------------------------------------------------------------------
 * SetColourMapEntries - these messages are only sent if the pixel
 * format uses a "colour map" (i.e. trueColour false) and the client has not
 * fixed the entire colour map using FixColourMapEntries.  In addition they
 * will only start being sent after the client has sent its first
 * FramebufferUpdateRequest.  So if the client always tells the server to use
 * trueColour then it never needs to process this type of message.
 */

typedef struct {
    CARD8 type;         /* always rfbSetColourMapEntries */
    CARD8 pad;
    CARD16 firstColour;
    CARD16 nColours;

    /* Followed by nColours * 3 * CARD16
       r1, g1, b1, r2, g2, b2, r3, g3, b3, ..., rn, bn, gn */

} rfbSetColourMapEntriesMsg;

#define sz_rfbSetColourMapEntriesMsg 6

/*****************************************************************************
 *
 * Bi-directional message types
 *
 *****************************************************************************/

/*-----------------------------------------------------------------------------
 * EnableExtension - tell client/server that a particular extension is available,
 * or reconfigure an extension.  If the new_msg field is non-zero then the sender
 * is indicating that the specified protocol uses the given message number.
 * The recipient may then send rfbExtensionData messages on the given message
 * number, provided the extension configuration data is recognised.
 *
 * The length field in the ExtensionData message may be invalid, if the zeroeth
 * bit of "flags" was not set in the corresponding EnableExtensionRequest.
 * If the length field is invalid then obviously the message can't be dealt
 * with by dumb proxies - avoid this where possible.
 *
 * The first bit of the flags field indicates that the extension is a new, named
 * encoding.  This form is used by servers to tell clients the names of the
 * encodings they support.  Typically, a receiving client will then dispatch
 * an "enable encoding" message for the specified encoding number, if supported.
 */
typedef struct {
    CARD8 type;         /* always rfbEnableExtensionRequest */
    CARD8 new_msg;
    CARD8 flags;
    CARD8 pad1;
    CARD32 length;
    /* Followed by <length> bytes of data */
} rfbEnableExtensionRequestMsg;

#define sz_rfbEnableExtensionRequestMsg 8

typedef struct {
    CARD8 type;         /* always >= rfbExtensionData */
    CARD8 pad1;
    CARD16 pad2;
    CARD32 length;      /* Must be correct if used */
    /* Followed by <length> bytes of data */
} rfbExtensionDataMsg;

#define sz_rfbExtensionDataMsg 8

/*-----------------------------------------------------------------------------
 * Bell - ring a bell on the client if it has one.
 */

typedef struct {
    CARD8 type;         /* always rfbBell */
} rfbBellMsg;

#define sz_rfbBellMsg 1



/*-----------------------------------------------------------------------------
 * ServerCutText - the server has new text in its cut buffer.
 */

typedef struct {
    CARD8 type;         /* always rfbServerCutText */
    CARD8 pad1;
    CARD16 pad2;
    CARD32 length;
    /* followed by char text[length] */
} rfbServerCutTextMsg;

#define sz_rfbServerCutTextMsg 8

/*-----------------------------------------------------------------------------
 * // Modif sf@2002
 * FileTransferMsg - The client sends FileTransfer message.
 * Bidirectional message - Files can be sent from client to server & vice versa
 */

typedef struct _rfbFileTransferMsg {
    CARD8 type;         /* always rfbFileTransfer */
    CARD8 contentType;  /* See defines below */
    CARD16 contentParam;/* Other possible content classification (Dir or File name, etc..) */
    CARD32 size;        /* FileSize or packet index or error or other  */
    CARD32 length;
    /* followed by data char text[length] */
} rfbFileTransferMsg;

#define sz_rfbFileTransferMsg   12

/* FileTransfer Content types and Params defines */
#define rfbDirContentRequest    1 /* Client asks for the content of a given Server directory */
#define rfbDirPacket            2 /* Full directory name or full file name. */
                                  /* Null content means end of Directory */
#define rfbFileTransferRequest  3 /* Client asks the server for the tranfer of a given file */
#define rfbFileHeader           4 /* First packet of a file transfer, containing file's features */
#define rfbFilePacket           5 /* One slice of the file */
#define rfbEndOfFile            6 /* End of file transfer (the file has been received or error) */
#define rfbAbortFileTransfer    7 /* The file transfer must be aborted, whatever the state */
#define rfbFileTransferOffer    8 /* The client offers to send a file to the server */
#define rfbFileAcceptHeader     9 /* The server accepts or rejects the file */
#define rfbCommand              10 /* The Client sends a simple command (File Delete, Dir create etc...) */
#define rfbCommandReturn        11 /* The Client receives the server's answer about a simple command */

                                /* rfbDirContentRequest client Request - content params  */
#define rfbRDirContent          1 /* Request a Server Directory contents */
#define rfbRDrivesList          2 /* Request the server's drives list */

                                /* rfbDirPacket & rfbCommandReturn  server Answer - content params */
#define rfbADirectory           1 /* Reception of a directory name */
#define rfbAFile                2 /* Reception of a file name  */
#define rfbADrivesList          3 /* Reception of a list of drives */
#define rfbADirCreate           4 /* Response to a create dir command  */
#define rfbADirDelete           5 /* Response to a delete dir command  */
#define rfbAFileCreate          6 /* Response to a create file command  */
#define rfbAFileDelete          7 /* Response to a delete file command  */

                                /* rfbCommand Command - content params */
#define rfbCDirCreate           1 /* Request the server to create the given directory */
#define rfbCDirDelete           2 /* Request the server to delete the given directory */
#define rfbCFileCreate          3 /* Request the server to create the given file */
#define rfbCFileDelete          4 /* Request the server to delete the given file */

                                /* Errors - content params or "size" field */
#define rfbRErrorUnknownCmd     1  /* Unknown FileTransfer command. */
#define rfbRErrorCmd            0xFFFFFFFF/* Error when a command fails on remote side (ret in "size" field) */

#define sz_rfbBlockSize         4096 /* Size of a File Transfer packet (before compression) */



/*-----------------------------------------------------------------------------
 * Modif sf@2002
 * TextChatMsg - Utilized to order the TextChat mode on server or client
 * Bidirectional message
 */

typedef struct _rfbTextChatMsg {
    CARD8 type;         /* always rfbTextChat */
    CARD8 pad1;         /* Could be used later as an additionnal param */
    CARD16 pad2;        /* Could be used later as text offset, for instance */
    CARD32 length;      /* Specific values for Open, close, finished (-1, -2, -3) */
    /* followed by char text[length] */
} rfbTextChatMsg;

#define sz_rfbTextChatMsg 8

#define rfbTextMaxSize      4096
#define rfbTextChatOpen     0xFFFFFFFF 
#define rfbTextChatClose    0xFFFFFFFE  
#define rfbTextChatFinished 0xFFFFFFFD  



/*-----------------------------------------------------------------------------
 * Modif sf@2002
 * ResizeFrameBuffer - The Client must change the size of its framebuffer  
 */

typedef struct _rfbResizeFrameBufferMsg {
    CARD8 type;         /* always rfbResizeFrameBuffer */
    CARD8 pad1;
    CARD16 framebufferWidth;    /* FrameBuffer width */
    CARD16 framebufferHeigth;   /* FrameBuffer height */
} rfbResizeFrameBufferMsg;

#define sz_rfbResizeFrameBufferMsg 6


/*-----------------------------------------------------------------------------
 * Copyright (C) 2001 Harakan Software
 * PalmVNC 1.4 & 2.? ResizeFrameBuffer message
 * ReSizeFrameBuffer - tell the RFB client to alter its framebuffer, either
 * due to a resize of the server desktop or a client-requested scaling factor.
 * The pixel format remains unchanged.
 */

typedef struct {
    CARD8 type;         /* always rfbReSizeFrameBuffer */
    CARD8 pad1;
    CARD16 desktop_w;   /* Desktop width */
    CARD16 desktop_h;   /* Desktop height */
    CARD16 buffer_w;    /* FrameBuffer width */
    CARD16 buffer_h;    /* Framebuffer height */
    CARD16 pad2;

} rfbPalmVNCReSizeFrameBufferMsg;

#define sz_rfbPalmVNCReSizeFrameBufferMsg (12)

/*-----------------------------------------------------------------------------
 * Union of all server->client messages.
 */

typedef union {
    CARD8 type;
    rfbFramebufferUpdateMsg fu;
    rfbSetColourMapEntriesMsg scme;
    rfbBellMsg b;
    rfbServerCutTextMsg sct;
    rfbEnableExtensionRequestMsg eer;
    rfbExtensionDataMsg ed;
    rfbResizeFrameBufferMsg rsfb;
    rfbPalmVNCReSizeFrameBufferMsg prsfb; 
    rfbFileTransferMsg ft;
    rfbTextChatMsg tc;
} rfbServerToClientMsg;



/*****************************************************************************
 *
 * Message definitions (client -> server)
 *
 *****************************************************************************/


/*-----------------------------------------------------------------------------
 * SetPixelFormat - tell the RFB server the format in which the client wants
 * pixels sent.
 */

typedef struct {
    CARD8 type;         /* always rfbSetPixelFormat */
    CARD8 pad1;
    CARD16 pad2;
    rfbPixelFormat format;
} rfbSetPixelFormatMsg;

#define sz_rfbSetPixelFormatMsg (sz_rfbPixelFormat + 4)


/*-----------------------------------------------------------------------------
 * FixColourMapEntries - when the pixel format uses a "colour map", fix
 * read-only colour map entries.
 *
 *    ***************** NOT CURRENTLY SUPPORTED *****************
 */

typedef struct {
    CARD8 type;         /* always rfbFixColourMapEntries */
    CARD8 pad;
    CARD16 firstColour;
    CARD16 nColours;

    /* Followed by nColours * 3 * CARD16
       r1, g1, b1, r2, g2, b2, r3, g3, b3, ..., rn, bn, gn */

} rfbFixColourMapEntriesMsg;

#define sz_rfbFixColourMapEntriesMsg 6


/*-----------------------------------------------------------------------------
 * SetEncodings - tell the RFB server which encoding types we accept.  Put them
 * in order of preference, if we have any.  We may always receive raw
 * encoding, even if we don't specify it here.
 */

typedef struct {
    CARD8 type;         /* always rfbSetEncodings */
    CARD8 pad;
    CARD16 nEncodings;
    /* followed by nEncodings * CARD32 encoding types */
} rfbSetEncodingsMsg;

#define sz_rfbSetEncodingsMsg 4


/*-----------------------------------------------------------------------------
 * FramebufferUpdateRequest - request for a framebuffer update.  If incremental
 * is true then the client just wants the changes since the last update.  If
 * false then it wants the whole of the specified rectangle.
 */

typedef struct {
    CARD8 type;         /* always rfbFramebufferUpdateRequest */
    CARD8 incremental;
    CARD16 x;
    CARD16 y;
    CARD16 w;
    CARD16 h;
} rfbFramebufferUpdateRequestMsg;

#define sz_rfbFramebufferUpdateRequestMsg 10


/*-----------------------------------------------------------------------------
 * KeyEvent - key press or release
 *
 * Keys are specified using the "keysym" values defined by the X Window System.
 * For most ordinary keys, the keysym is the same as the corresponding ASCII
 * value.  Other common keys are:
 *
 * BackSpace        0xff08
 * Tab          0xff09
 * Return or Enter  0xff0d
 * Escape       0xff1b
 * Insert       0xff63
 * Delete       0xffff
 * Home         0xff50
 * End          0xff57
 * Page Up      0xff55
 * Page Down        0xff56
 * Left         0xff51
 * Up           0xff52
 * Right        0xff53
 * Down         0xff54
 * F1           0xffbe
 * F2           0xffbf
 * ...          ...
 * F12          0xffc9
 * Shift        0xffe1
 * Control      0xffe3
 * Meta         0xffe7
 * Alt          0xffe9
 */

typedef struct {
    CARD8 type;         /* always rfbKeyEvent */
    CARD8 down;         /* true if down (press), false if up */
    CARD16 pad;
    CARD32 key;         /* key is specified as an X keysym */
} rfbKeyEventMsg;

#define sz_rfbKeyEventMsg 8


/*-----------------------------------------------------------------------------
 * PointerEvent - mouse/pen move and/or button press.
 */

typedef struct {
    CARD8 type;         /* always rfbPointerEvent */
    CARD8 buttonMask;       /* bits 0-7 are buttons 1-8, 0=up, 1=down */
    CARD16 x;
    CARD16 y;
} rfbPointerEventMsg;

#define rfbButton1Mask 1
#define rfbButton2Mask 2
#define rfbButton3Mask 4
#define rfbButton4Mask 8
#define rfbButton5Mask 16
#define rfbWheelUpMask rfbButton4Mask
#define rfbWheelDownMask rfbButton5Mask

#define sz_rfbPointerEventMsg 6



/*-----------------------------------------------------------------------------
 * ClientCutText - the client has new text in its cut buffer.
 */

typedef struct {
    CARD8 type;         /* always rfbClientCutText */
    CARD8 pad1;
    CARD16 pad2;
    CARD32 length;
    /* followed by char text[length] */
} rfbClientCutTextMsg;

#define sz_rfbClientCutTextMsg 8


/*-----------------------------------------------------------------------------
 * sf@2002 - Set Server Scale
 * SetServerScale - Server must change the scale of the client buffer.
 */

typedef struct _rfbSetScaleMsg {
    CARD8 type;         /* always rfbSetScale */
    CARD8 scale;        /* Scale value 1<sv<n */
    CARD16 pad;
} rfbSetScaleMsg;

#define sz_rfbSetScaleMsg 4


/*-----------------------------------------------------------------------------
 * Copyright (C) 2001 Harakan Software
 * PalmVNC 1.4 & 2.? SetScale Factor message 
 * SetScaleFactor - tell the RFB server to alter the scale factor for the
 * client buffer.
 */
typedef struct {
    CARD8 type;         /* always rfbSetScaleFactor */

    CARD8 scale;        /* Scale factor (positive non-zero integer) */
    CARD16 pad2;
} rfbPalmVNCSetScaleFactorMsg;

#define sz_rfbPalmVNCSetScaleFactorMsg (4)


/*-----------------------------------------------------------------------------
 * rdv@2002 - Set input status
 * SetServerInput - Server input is dis/enabled
 */

typedef struct _rfbSetServerInputMsg {
    CARD8 type;         /* always rfbSetScale */
    CARD8 status;       /* Scale value 1<sv<n */
    CARD16 pad;
} rfbSetServerInputMsg;

#define sz_rfbSetServerInputMsg 4

/*-----------------------------------------------------------------------------
 * rdv@2002 - Set SW
 * SetSW - Server SW/full desktop
 */

typedef struct _rfbSetSWMsg {
    CARD8 type;         /* always rfbSetSW */
    CARD8 status;       
    CARD16 x;
    CARD16 y;
} rfbSetSWMsg;

#define sz_rfbSetSWMsg 6


/*-----------------------------------------------------------------------------
 * Union of all client->server messages.
 */

typedef union {
    CARD8 type;
    rfbSetPixelFormatMsg spf;
    rfbFixColourMapEntriesMsg fcme;
    rfbSetEncodingsMsg se;
    rfbFramebufferUpdateRequestMsg fur;
    rfbKeyEventMsg ke;
    rfbPointerEventMsg pe;
    rfbClientCutTextMsg cct;
    rfbEnableExtensionRequestMsg eer;
    rfbExtensionDataMsg ed;
    rfbSetScaleMsg ssc;
    rfbPalmVNCSetScaleFactorMsg pssf;
    rfbSetServerInputMsg sim;
    rfbFileTransferMsg ft;
    rfbSetSWMsg sw;
    rfbTextChatMsg tc;
} rfbClientToServerMsg;
