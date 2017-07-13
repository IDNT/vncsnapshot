# Makefile for vncsnapshot, Unix/Linux platforms.
#
# $Id: Makefile,v 1.6 2004/09/09 00:22:33 grmcdorman Exp $

#
# INSTALLER: Adjust these lines to point to your installation's location of
# Zlib and JPEG lib.
ZLIB_INCLUDE = -I/usr/local/include
ZLIB_LIB = -L/usr/local/lib -lz
JPEG_INCLUDE = -I/usr/local/include
JPEG_LIB = -L/usr/local/lib -ljpeg


# Other libraries:
# SOLARIS:
# EXTRALIBS = -lsocket -lnsl
# EXTRAINCLUDES =
# Linux:
EXTRALIBS =
EXTRAINCLUDES =

# Compilation Flags. Season to taste.
CC = gcc
CDEBUGFLAGS = -O2 -Wall
CXX=g++
# You shouldn't need to change anything below.
INCLUDES = -I. $(ZLIB_INCLUDE) $(JPEG_INCLUDE) $(EXTRAINCLUDES)
CFLAGS =  $(CDEBUGFLAGS) $(INCLUDES)
CXXFLAGS = $(CFLAGS)

# Solaris 8 uses CCC and CCFLAGS
CCC=$(CXX)
CCFLAGS = $(CFLAGS)

.SUFFIXES: .cxx
#

SRCS = \
  argsresources.c \
  buffer.c \
  cursor.c \
  listen.c \
  rfbproto.c \
  sockets.cxx \
  tunnel.c \
  vncsnapshot.c \
  d3des.c vncauth.c \
  zrle.cxx \

PASSWD_SRCS =  vncpasswd.c vncauth.c d3des.c

OBJS1 = $(SRCS:.c=.o)
OBJS  = $(OBJS1:.cxx=.o)

PASSWD_OBJS1 = $(PASSWD_SRCS:.c=.o)
PASSWD_OBJS  = $(PASSWD_OBJS1:.cxx=.o)

SUBDIRS=rdr.dir

all: $(SUBDIRS:.dir=.all) vncsnapshot vncpasswd

vncsnapshot: $(OBJS)
#	${CXX} ${CXXFLAGS} ${LDFLAGS} -o $@ $(OBJS) rdr/librdr.a $(ZLIB_LIB) $(JPEG_LIB) $(EXTRALIBS)
	$(LINK.cc) $(CDEBUGFLAGS) -o $@ $(OBJS) rdr/librdr.a $(ZLIB_LIB) $(JPEG_LIB) $(EXTRALIBS)

vncpasswd: $(PASSWD_OBJS)
#	${CXX} ${CXXFLAGS} ${LDFLAGS} -o $@ $(PASSWD_OBJS)
	$(LINK.c) $(CDEBUGFLAGS) -o $@ $(PASSWD_OBJS)

clean: $(SUBDIRS:.dir=.clean) $(FINAL_SUBDIRS:.dir=.clean)
	-rm -f $(OBJS) $(PASSWD_OBJS) vncpasswd vncsnapshot core

reallyclean: clean $(SUBDIRS:.dir=.reallyclean) $(FINAL_SUBDIRS:.dir=.reallyclean)
	-rm -f *~

rdr.all:
	cd rdr;make all
rdr.clean:
	cd rdr;make clean
rdr.reallyclean:
	cd rdr;make reallyclean

vncpasswd.all:
	cd vncpasswd;make all
vncpasswd.clean:
	cd vncpasswd;make clean
vncpasswd.reallyclean:
	cd vncpasswd;make reallyclean

.cxx.o:
	$(COMPILE.cc) -o $@ $<
# dependancies

argsresources.o: argsresources.c vncsnapshot.h rfb.h rfbproto.h
buffer.o: buffer.c vncsnapshot.h rfb.h rfbproto.h
cursor.o: cursor.c vncsnapshot.h rfb.h rfbproto.h
listen.o: listen.c vncsnapshot.h rfb.h rfbproto.h
rfbproto.o: rfbproto.c vncsnapshot.h rfb.h rfbproto.h vncauth.h \
  protocols/rre.c protocols/corre.c \
  protocols/hextile.c protocols/zlib.c protocols/tight.c
sockets.o: sockets.cxx vncsnapshot.h rfb.h rfbproto.h
tunnel.o: tunnel.c vncsnapshot.h rfb.h rfbproto.h
vncsnapshot.o: vncsnapshot.c vncsnapshot.h rfb.h rfbproto.h
vncauth.o: vncauth.c stdhdrs.h rfb.h rfbproto.h vncauth.h d3des.h
zrle.o: zrle.cxx vncsnapshot.h
vncpasswd.o: vncpasswd.c vncauth.h
