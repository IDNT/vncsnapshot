# Makefile for vncsnapshot, Unix/Linux platforms.
#
# $Id: Makefile,v 1.2 2004/09/09 00:22:36 grmcdorman Exp $


# You shouldn't need to change anything below.
INCLUDES = -I. -I.. $(ZLIB_INCLUDE) $(JPEG_INCLUDE) $(EXTRAINCLUDES)
CFLAGS =  $(CDEBUGFLAGS) $(INCLUDES)
CXXFLAGS = $(CFLAGS)
# Solaris 8 uses CCFLAGS instead of CXXFLAGS
CCFLAGS = $(CXXFLAGS)

.SUFFIXES: .cxx

SRCS = \
FdInStream.cxx	 InStream.cxx	    ZlibInStream.cxx \
FdOutStream.cxx  NullOutStream.cxx  ZlibOutStream.cxx \

OBJS1 = $(SRCS:.c=.o)
OBJS  = $(OBJS1:.cxx=.o)

SUBDIRS=
TARGET=librdr.a

all: $(SUBDIRS:.dir=.all) $(TARGET)

$(TARGET): $(OBJS)
	$(AR) cr $(TARGET) $(OBJS)
	ranlib $(TARGET)


clean: $(SUBDIRS:.dir=.clean)
	-rm -f $(OBJS) $(TARGET) core

reallyclean: clean $(SUBDIRS:.=.reallyclean)
	-rm -f *~


.cxx.o:
	$(COMPILE.cc) -o $@ $<
#.o.cxx:
	echo $@ $<
# dependancies
