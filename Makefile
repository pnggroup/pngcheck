# Makefile for pngcheck by Greg Roelofs et al.
#
# This makefile assumes zlib has either been built in a subdirectory at the
# same level as the current subdirectory (as indicated by the ZLIB_PATH macro
# below), or installed system-wide.  Edit the ZLIB_* macros as appropriate.


# macros --------------------------------------------------------------------

ZLIB_PATH = ../zlib
ZLIB_INCPATH = $(ZLIB_PATH)
ZLIB_LIBPATH = $(ZLIB_PATH)
ZLIB_LIB = -lz

CC = cc # gcc or clang
LD = $(CC)
RM_F = rm -f

STDC = -pedantic-errors # -std=c99 or -std=c11 or -std=c17 etc.
WARN = -Wall -Wextra -Wundef
WARNMORE = -Wcast-align -Wconversion -Wpointer-arith -Wwrite-strings \
           -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes \
           -Wno-implicit-fallthrough
LOCAL_CPPFLAGS =
LOCAL_CFLAGS = $(STDC) $(WARN) # $(WARNMORE)
LOCAL_LDFLAGS =

CPPFLAGS = -I$(ZLIB_INCPATH) -DUSE_ZLIB
CFLAGS = -O2 # -g
LDFLAGS = -L$(ZLIB_LIBPATH) # -g
LIBS = $(ZLIB_LIB)

ALL_CPPFLAGS = $(LOCAL_CPPFLAGS) $(CPPFLAGS)
ALL_CFLAGS = $(LOCAL_CFLAGS) $(CFLAGS)
ALL_LDFLAGS = $(LOCAL_LDFLAGS) $(LDFLAGS)

EXEEXT = # .exe
EXES = pngcheck$(EXEEXT)
OBJS = pngcheck.o


# implicit make rules -------------------------------------------------------

.c.o:
	$(CC) -c $(ALL_CPPFLAGS) $(ALL_CFLAGS) -o $@ $*.c


# dependencies --------------------------------------------------------------

all: $(EXES)

pngcheck$(EXEEXT): pngcheck.o
	$(LD) $(ALL_LDFLAGS) -o $@ pngcheck.c $(LIBS)


# maintenance ---------------------------------------------------------------

clean:
	$(RM_F) $(EXES) $(OBJS)
