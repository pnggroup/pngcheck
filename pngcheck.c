/*
 * pngcheck:  Authenticate the structure of a PNG file and dump info about
 *            it if desired.
 *
 * This program checks the PNG identifier with conversion checks,
 * the file structure and the chunk CRCs. In addition, relations and contents
 * of most defined chunks are checked, except for decompression of the IDAT
 * chunks.
 *
 *        NOTE:  this program is currently NOT EBCDIC-compatible!
 *               (as of September 2000)
 *
 * ChangeLog:  see CHANGELOG file
 */

/*============================================================================
 *
 *   Copyright 1995-2000 by Alexander Lehmann <lehmann@usa.net>,
 *                          Andreas Dilger <adilger@enel.ucalgary.ca>,
 *                          Glenn Randers-Pehrson <randeg@alum.rpi.edu>,
 *                          Greg Roelofs <newt@pobox.com>,
 *                          John Bowler <jbowler@acm.org>,
 *                          Tom Lane <tgl@sss.pgh.pa.us>
 *  
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted, provided
 *   that the above copyright notice appear in all copies and that both that
 *   copyright notice and this permission notice appear in supporting
 *   documentation.  This software is provided "as is" without express or
 *   implied warranty.
 *
 *===========================================================================*/

#define VERSION "1.99.3 of 2 September 2000"

/*
 * GRR NOTE:  current MNG support is informational; error-checking is MINIMAL!
 * 
 *
 * GRR to do:
 *   - update existing MNG support to Draft 70 (version 0.97) or later
 *   - MNG chunks:  PAST, DISC, tERm, DROP, DBYK, ORDR
 *   - add JNG restrictions to bKGD
 *   - report illegal Macromedia Fireworks pRVW chunk?
 *   - add chunk-split option
 *       (pngcheck -c foo.png -> foo.000.sig, foo.001.IHDR, foo.002.PLTE, etc.)
 *   - split out each chunk's code into XXXX() function (e.g., IDAT(), tRNS())
 *   - with USE_ZLIB, print zTXt chunks if -t option
 *   - DOS/Win32 wildcard support beyond emx+gcc (wildargs.obj?)
 *   - EBCDIC support (minimal?)
 */

/*
 * Compilation example (GNU C, command line; replace "zlibpath" as appropriate):
 *
 *    without zlib:
 *       gcc -O -o pngcheck pngcheck.c
 *    with zlib support:
 *       gcc -O -DUSE_ZLIB -I/zlibpath -o pngcheck pngcheck.c -L/zlibpath -lz
 *    or (static zlib):
 *       gcc -O -DUSE_ZLIB -I/zlibpath -o pngcheck pngcheck.c /zlibpath/libz.a
 *
 * Windows compilation example (MSVC, command line, assuming VCVARS32.BAT or
 * whatever has been run):
 *
 *    without zlib:
 *       cl -nologo -O -W3 -DWIN32 pngcheck.c
 *    with zlib support (note that Win32 zlib is compiled as a DLL by default):
 *       cl -nologo -O -W3 -DWIN32 -DUSE_ZLIB -I/zlibpath -c pngcheck.c
 *       link -nologo pngcheck.obj setargv.obj \zlibpath\zlib.lib
 *       [copy pngcheck.exe and zlib.dll to installation directory]
 *
 * "setargv.obj" is included with MSVC and will be found if the batch file has
 * been run.  Either Borland or Watcom (both?) may use "wildargs.obj" instead.
 * Both object files serve the same purpose:  they expand wildcard arguments
 * into a list of files on the local file system, just as Unix shells do by
 * default ("globbing").  Note that mingw32 + gcc (Unix-like compilation
 * environment for Windows) apparently expands wildcards on its own, so no
 * special object files are necessary for it.  emx + gcc for OS/2 (and possibly
 * rsxnt + gcc for Windows NT) has a special _wildcard() function call, which
 * is already included at the top of main() below.
 *
 * zlib info:		http://www.info-zip.org/pub/infozip/zlib/
 * PNG/MNG/JNG info:	http://www.libpng.org/pub/png/
 *			http://www.libpng.org/pub/mng/  and
 *                      ftp://swrinde.nde.swri.edu/pub/mng/
 * pngcheck sources:	http://www.libpng.org/pub/png/apps/pngcheck.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#ifdef WIN32
#  include <io.h>
#endif
#ifdef USE_ZLIB
#  include "zlib.h"
#endif

typedef unsigned char  uch;
typedef unsigned short ush;
typedef unsigned long  ulg;

int check_magic(uch *magic, char *fname, int which);
int check_chunk_name(char *chunk_name, char *fname);
void make_crc_table(void);
ulg update_crc(ulg crc, uch *buf, int len);
ulg getlong(FILE *fp, char *fname, char *where);
void init_printbuffer(char *fname);
void printbuffer(char *buffer, int size, int indent);
void finish_printbuffer(char *fname, char *chunkid);
int keywordlen(char *buffer, int maxsize);
char *getmonth(int m);
void pngcheck(FILE *fp, char *_fname, int searching, FILE *fpOut);

#define BS 32000 /* size of read block for CRC calculation (and zlib) */

/* Mark's macros to extract big-endian short and long ints: */
#define SH(p) ((ush)(uch)((p)[1]) | ((ush)(uch)((p)[0]) << 8))
#define LG(p) ((ulg)(SH((p)+2)) | ((ulg)(SH(p)) << 16))

/* for check_magic(): */
#define DO_PNG  0
#define DO_MNG  1
#define DO_JNG  2

#define set_err(x) error = error < (x) ? (x) : error
#define is_err(x)  ((error > (x) || (!force && error == (x))) ? 1 : 0)
#define no_err(x)  ((error < (x) || (force && error == (x))) ? 1 : 0)

/* Command-line flag variables */
int verbose = 0;	/* print chunk info */
int quiet = 0;		/* only print error messages */
int printtext = 0;	/* print tEXt chunks */
int printpal = 0;	/* print PLTE/tRNS/hIST/sPLT contents */
int sevenbit = 0;	/* escape characters >=160 */
int force = 0;		/* continue even if an occurs (CRC error, etc) */
int search = 0;		/* hunt for PNGs in the file... */
int extract = 0;	/* ...and extract them to arbitrary file names. */
int png = 0;		/* it's a PNG */
int mng = 0;		/* it's a MNG instead of a PNG (won't work in pipe) */
int jng = 0;		/* it's a JNG */

int error; /* the current error status */
uch buffer[BS];

/* What the PNG, MNG and JNG magic numbers should be */
static uch good_PNG_magic[8] = {137, 80, 78, 71, 13, 10, 26, 10};
static uch good_MNG_magic[8] = {138, 77, 78, 71, 13, 10, 26, 10};
static uch good_JNG_magic[8] = {139, 74, 78, 71, 13, 10, 26, 10};

/* table of CRCs of all 8-bit messages */
ulg crc_table[256];

/* Flag: has the table been computed? Initially false. */
int crc_table_computed = 0;

#ifdef USE_ZLIB
  int first_idat = 1;   /* flag: is this the first IDAT chunk? */
  int zlib_error = 0;
  uch outbuf[BS];
  z_stream zstrm;
#endif


static char *inv = "INVALID";


/* PNG stuff */

static char *png_type[] = {
  "grayscale",
  "INVALID",	/* inv? */
  "RGB",
  "colormap",
  "grayscale+alpha",
  "INVALID",
  "RGB+alpha"
};

static char *eqn_type[] = {
  "physical_value = p0 + p1 * original_sample / (x1-x0)",
  "physical_value = p0 + p1 * exp(p2 * original_sample / (x1-x0))",
  "physical_value = p0 + p1 * pow(p2, (original_sample / (x1-x0)))",
  "physical_value = p0 + p1 * sinh(p2 * (original_sample - p3) / (x1-x0))"
};

static int eqn_params[] = { 2, 3, 3, 4 };   /* pCAL */


/* JNG stuff */

static char *jng_type[] = {
  "grayscale",
  "YCbCr",
  "grayscale+alpha",
  "YCbCr+alpha"
};


/* MNG stuff */

static char *rendering_intent[] = {
  "perceptual",
  "relative colorimetric",
  "saturation-preserving",
  "absolute colorimetric"
};

static char *delta_type[] = {
  "full image replacement",
  "block pixel addition",
  "block alpha addition",
  "block pixel replacement",
  "block alpha replacement",
  "no change"
};

static char *framing_mode[] = {
  "no change in framing mode",
  "no background layer; interframe delay before each image displayed",
  "no background layer; interframe delay before each FRAM chunk",
  "interframe delay and background layer before each image displayed",
  "interframe delay and background layer after each FRAM chunk"
/*
  OLD (~ Draft 43):
  "individual images with delays and background restoration (default)",
  "composite frame with final delay and initial background restoration",
  "individual images with delays and initial background restoration",
  "individual images with delays and no background restoration",
  "composite frame with final delay and no background restoration"
 */
};

static char *termination_condition[] = {
  "deterministic",
  "decoder discretion",
  "user discretion",
  "external signal"
};

static char *change_interframe_delay[] = {
  "no change in interframe delay",
  "change interframe delay for next subframe",
  "change interframe delay and make default"
};

static char *change_timeout_and_termination[] = {
  "no change in timeout and termination",
  "deterministic change in timeout and termination for next subframe",
  "deterministic change in timeout and termination; make default",
  "decoder-discretion change in timeout and termination for next subframe",
  "decoder-discretion change in timeout and termination; make default",
  "user-discretion change in timeout and termination for next subframe",
  "user-discretion change in timeout and termination; make default",
  "change in timeout and termination for next subframe via signal",
  "change in timeout and termination via signal; make default"
};

static char *change_subframe_clipping_boundaries[] = {
  "no change in subframe clipping boundaries",
  "change frame clipping boundaries for next subframe",
  "change frame clipping boundaries and make default"
};

static char *change_sync_id_list[] = {
  "no change in sync ID list",
  "change sync ID list for next subframe:",
  "change sync ID list and make default:"
};

static char *clone_type[] = {
  "full",
  "partial",
  "renumber"
};

static char *do_not_show[] = {
  "potentially visible",
  "do not show",
  "same visibility as parent"
};

static char *show_mode[] = {
  "make objects potentially visible and display",
  "make objects invisible",
  "display potentially visible objects",
  "make objects potentially visible but do not display",
  "toggle potentially visible and invisible objects; display visible ones",
  "toggle potentially visible and invisible objects but do not display any",
  "make next object potentially visible and display; make rest invisible",
  "make next object potentially visible but do not display; make rest invisible"
};

static char *entry_type[] = {
  "INVALID",
  "segment",
  "frame",
  "exported image"
};

static char *pplt_delta_type[] = {
  "replacement RGB samples",
  "delta RGB samples",
  "replacement alpha samples",
  "delta alpha samples",
  "replacement RGBA samples",
  "delta RGBA samples"
};


/* make the table for a fast crc */
void make_crc_table(void)
{
  int n;

  for (n = 0; n < 256; n++) {
    ulg c;
    int k;

    c = (ulg)n;
    for (k = 0; k < 8; k++)
      c = c & 1 ? 0xedb88320L ^ (c >> 1):c >> 1;

    crc_table[n] = c;
  }
  crc_table_computed = 1;
}

/* update a running crc with the bytes buf[0..len-1]--the crc should be
   initialized to all 1's, and the transmitted value is the 1's complement
   of the final running crc. */

ulg update_crc(ulg crc, uch *buf, int len)
{
  ulg c = crc;
  uch *p = buf;
  int n = len;

  if (!crc_table_computed) {
    make_crc_table();
  }

  if (n > 0) do {
    c = crc_table[(c ^ (*p++)) & 0xff] ^ (c >> 8);
  } while (--n);
  return c;
}

/* use these instead of ~crc and -1, since that doesn't work on machines that
   have 64 bit longs */

#define CRCCOMPL(c) ((c)^0xffffffff)
#define CRCINIT (CRCCOMPL(0))

ulg getlong(FILE *fp, char *fname, char *where)
{
  ulg res=0;
  int i;

  for(i=0;i<4;i++) {
    int c;

    if ((c=fgetc(fp))==EOF) {
      printf("%s  EOF while reading %s\n", verbose? ":":fname, where);
      set_err(3);
      return 0;
    }
    res<<=8;
    res|=c&0xff;
  }
  return res;
}

/* output a long when copying an embedded PNG out of a file. */
void putlong(FILE *fpOut, ulg ul) {
  putc(ul >> 24, fpOut);
  putc(ul >> 16, fpOut);
  putc(ul >> 8, fpOut);
  putc(ul, fpOut);
}

/* print out size characters in buffer, take care to not print control chars
   other than whitespace, since this may open ways of attack by so-called
   ANSI-bombs */

int cr;
int lf;
int nul;
int control;
int esc;

void init_printbuffer(char *fname)
{
  cr=0;
  lf=0;
  nul=0;
  control=0;
  esc=0;
}

void printbuffer(char *buffer, int size, int indent)	/* GRR EBCDIC WARNING */
{
  if (indent)
    printf("    ");
  while (size--) {
    uch c;

    c = *buffer++;

    if ((c < ' ' && c != '\t' && c != '\n') ||
        (sevenbit? c > 127 : (c >= 127 && c < 160)))
      printf("\\%02X", c);
/*
    else if (c == '\\')
      printf("\\\\");
 */
    else
      putchar(c);

    if (c < 32 || (c >= 127 && c < 160)) {
      if (c == '\n') {
        lf = 1;
        if (indent && size > 0)
          printf("    ");
      } else if (c == '\r')
        cr = 1;
      else if (c == '\0')
        nul = 1;
      else
        control = 1;
      if (c == 27)
        esc = 1;
    }
  }
}

void finish_printbuffer(char *fname, char *chunkid)
{
  if (cr) {
    if (lf) {
      printf("%s  %s chunk contains both CR and LF as line terminators\n",
             verbose? "":fname, chunkid);
      set_err(1);
    } else {
      printf("%s  %s chunk contains only CR as line terminator\n",
             verbose? "":fname, chunkid);
      set_err(1);
    }
  }
  if (nul) {
    printf("%s  %s chunk contains null bytes\n", verbose? "":fname, chunkid);
    set_err(1);
  }
  if (control) {
    printf("%s  %s chunk contains one or more control characters%s\n",
           verbose? "":fname, chunkid, esc? " including Escape":"");
    set_err(1);
  }
}

int keywordlen(char *buffer, int maxsize)
{
  int i;

  for(i=0;i<maxsize && buffer[i]; i++);

  return i;
}

char *getmonth(int m)
{
  static char *month[] =
  {"(undefined)", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  if (m < 1 || m > 12)
    return month[0];
  else
    return month[m];
}

int ratio(uc, c)         /* GRR 970621:  swiped from UnZip 5.31 list.c */
    ulg uc, c;
{
    ulg denom;

    if (uc == 0)
        return 0;
    if (uc > 2000000L) {    /* risk signed overflow if multiply numerator */
        denom = uc / 1000L;
        return ((uc >= c) ?
            (int) ((uc-c + (denom>>1)) / denom) :
          -((int) ((c-uc + (denom>>1)) / denom)));
    } else {             /* ^^^^^^^^ rounding */
        denom = uc;
        return ((uc >= c) ?
            (int) ((1000L*(uc-c) + (denom>>1)) / denom) :
          -((int) ((1000L*(c-uc) + (denom>>1)) / denom)));
    }                            /* ^^^^^^^^ rounding */
}

void print_zlibheader(ulg uhead)
{
  /* See the code in zlib deflate.c that writes out the header when s->status
     is INIT_STATE.  In fact this code is based on the zlib specification in
     RFC 1950, i.e., ftp://ds.internic.net/rfc/rfc1950.txt, with the implicit
     assumption that the zlib header *is* written (it always should be inside
     a valid PNG file)  The variable names are taken, verbatim, from the RFC.
   */
  unsigned int CM = (uhead & 0xf00) >> 8;
  unsigned int CINFO = (uhead & 0xf000) >> 12;
  unsigned int FDICT = (uhead & 0x20) >> 5;
  unsigned int FLEVEL = (uhead & 0xc0) >> 6;

  if (uhead % 31) {
    printf("   zlib:  compression header fails checksum\n");
    set_err(2);
  } else if (CM == 8) {
    static char *flevel[4] = {"superfast", "fast", "default", "maximum"};

    if (CINFO > 1)
      printf("    zlib:  deflated, %dK window, %s compression%s\n",
        (1 << (CINFO-2)), flevel[FLEVEL], FDICT? ", preset dictionary":"");
    else
      printf("    zlib:  deflated, %d-byte window, %s compression%s\n",
        (1 << (CINFO+8)), flevel[FLEVEL], FDICT? ", preset dictionary":"");
  } else {
    printf("    zlib:  non-deflate compression method (%d)\n", CM);
    set_err(2);
  }
}

void pngcheck(FILE *fp, char *fname, int searching, FILE *fpOut)
{
  int i, j;
  long sz;
  uch magic[8];
  char chunkid[5] = {'\0', '\0', '\0', '\0', '\0'};
  char *and = "";
  int toread;
  int c;
  int ihdr_read = 0, iend_read = 0;
  int mhdr_read = 0, mend_read = 0;
  int dhdr_read = 0, plte_read = 0;
  int jhdr_read = 0, jsep_read = 0, need_jsep = 0;
  int idat_read = 0, last_is_idat = 0, jdat_read = 0, last_is_jdat = 0;
  int have_bkgd = 0, have_chrm = 0, have_gama = 0, have_hist = 0, have_iccp = 0;
  int have_offs = 0, have_pcal = 0, have_phys = 0, have_sbit = 0, have_scal = 0;
  int have_srgb = 0, have_time = 0, have_trns = 0;
  ulg crc, filecrc;
  ulg zhead = 1;   /* 0x10000 indicates both zlib header bytes read. */
  ulg layers = 0L, frames = 0L;
  long num_chunks = 0L;
  long w = 0L, h = 0L;
  long mng_width = 0L, mng_height = 0L;
  int vlc = -1, lc = -1;
  int bitdepth = 0, ityp = 0, jtyp = 0, lace = 0, nplte = 0;
  int jbitd = 0, alphadepth = 0;
  int did_stat = 0;
  struct stat statbuf;
  static int first_file = 1;

  error = 0;

  if (verbose || printtext || printpal) {
    printf("%sFile: %s", first_file? "":"\n", fname);

    if (searching) {
      printf("\n");
    } else {
      stat(fname, &statbuf);   /* know file exists => know stat() successful */
      did_stat = 1;
      printf(" (%ld bytes)\n", statbuf.st_size);
    }
  }

  first_file = 0;
  png = mng = jng = 0;

  if (!searching) {
    int check = 0;

    if (fread(magic, 1, 8, fp)!=8) {
      printf("%s  cannot read PNG or MNG signature\n", verbose? "":fname);
      set_err(3);
      return;
    }

    if (magic[0]==0 && magic[1]>0 && magic[1]<=64 && magic[2]!=0) {
      if (!quiet)
        printf("%s  (trying to skip MacBinary header)\n", verbose? "":fname);

      if (fread(buffer, 1, 120, fp) == 120 && fread(magic, 1, 8, fp) == 8) {
        printf("    cannot read MacBinary header\n");
        set_err(3);
      } else if ((check = check_magic(magic, fname, DO_PNG)) == 0) {
        png = 1;
        if (!quiet)
          printf("    this PNG seems to be contained in a MacBinary file\n");
      } else if ((check = check_magic(magic, fname, DO_MNG)) == 0) {
        mng = 1;
        if (!quiet)
          printf("    this MNG seems to be contained in a MacBinary file\n");
      } else if ((check = check_magic(magic, fname, DO_JNG)) == 0) {
        jng = 1;
        if (!quiet)
          printf("    this JNG seems to be contained in a MacBinary file\n");
      } else {
        if (check == 2)
          printf("    this is neither a PNG or JNG image nor MNG stream\n");
        set_err(2);
      }
    } else if ((check = check_magic(magic, fname, DO_PNG)) == 0)
      png = 1;
    else if (check == 2) {   /* see if it's a MNG or a JNG instead */
      if ((check = check_magic(magic, fname, DO_MNG)) == 0)
        mng = 1;        /* yup */
      else if (check == 2 && (check = check_magic(magic, fname, DO_JNG)) == 0)
        jng = 1;        /* yup */
      else {
        set_err(2);
        if (check == 2)
          printf("%s  this is neither a PNG or JNG image nor a MNG stream\n",
            verbose? "":fname);
      }
    }

    if (is_err(1))
      return;
  }

  /*-------------------- BEGINNING OF IMMENSE WHILE-LOOP --------------------*/

  while ((c = fgetc(fp)) != EOF) {
    ungetc(c, fp);

    if (png && iend_read || mng && mend_read) {
      if (searching) /* Start looking again in the current file. */
        return;

      if (!quiet)
        printf("%s  additional data after %cEND chunk\n", verbose? "":fname,
          mng? 'M':'I');

      set_err(1);
      if (!force)
        return;
    }

    sz = getlong(fp, fname, "chunk length");

    if (is_err(2))
      return;

    if (fread(chunkid, 1, 4, fp) != 4) {
      printf("%s  EOF while reading chunk type\n", verbose? ":":fname);
      set_err(3);
      return;
    }

    /* GRR:  add 4-character EBCDIC conversion here (chunkid) */

    chunkid[4] = '\0';
    ++num_chunks;

    if (check_chunk_name(chunkid, fname) != 0) {
      set_err(2);
      if (!force)
        return;
    }

    if (verbose)
      printf("  chunk %s at offset 0x%05lx, length %ld", chunkid,
             ftell(fp)-4, sz);

    if (is_err(1))
      return;

    crc = update_crc(CRCINIT, (uch *)chunkid, 4);

    if ((png && !ihdr_read && strcmp(chunkid,"IHDR")!=0) ||
        (mng && !mhdr_read && strcmp(chunkid,"MHDR")!=0) ||
        (jng && !jhdr_read && strcmp(chunkid,"JHDR")!=0))
    {
      printf("%s  first chunk must be %cHDR\n",
             verbose? ":":fname, png? 'I' : (mng? 'M':'J'));
      set_err(1);
      if (!force)
        return;
    }

    toread = (sz > BS)? BS:sz;

    if (fread(buffer, 1, (size_t)toread, fp) != (size_t)toread) {
      printf("%s  EOF while reading %s%sdata\n",
             verbose? ":":fname, verbose? "":chunkid, verbose? "":" ");
      set_err(3);
      return;
    }

    crc = update_crc(crc, (uch *)buffer, toread);

    /*================================*
     * PNG, JNG and MNG header chunks *
     *================================*/

    /*------* 
     | IHDR | 
     *------*/
    if (strcmp(chunkid, "IHDR") == 0) {
      if (png && ihdr_read) {
        printf("%s  multiple IHDR not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (sz != 13) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"IHDR ");
        set_err(2);
      }
      if (no_err(1)) {
        w = LG(buffer);
        h = LG(buffer+4);
        if (w == 0 || h == 0) {
          printf("%s  invalid %simage dimensions (%ldx%ld)\n",
            verbose? ":":fname, verbose? "":"IHDR ", w, h);
          set_err(1);
        }
        bitdepth = (uch)buffer[8];
        ityp = (uch)buffer[9];
        if (ityp > sizeof(png_type)/sizeof(char*)) {
          ityp = 1; /* avoid out of range array index */
        }
        switch (bitdepth) {
          case 1:
          case 2:
          case 4:
            if (ityp == 2 || ityp == 4 || ityp == 6) {/* RGB or GA or RGBA */
              printf("%s  invalid %sbit depth (%d) for %s image\n",
                verbose? ":":fname, verbose? "":"IHDR ", bitdepth,
                png_type[ityp]);
              set_err(1);
            }
            break;
          case 8:
            break;
          case 16:
            if (ityp == 3) { /* palette */
              printf("%s  invalid %sbit depth (%d) for %s image\n",
                verbose? ":":fname, verbose? "":"IHDR ", bitdepth,
                png_type[ityp]);
              set_err(1);
            }
            break;
          default:
            printf("%s  invalid %sbit depth (%d)\n",
              verbose? ":":fname, verbose? "":"IHDR ", bitdepth);
            set_err(1);
            break;
        }
        lace = (uch)buffer[12];
        switch (ityp) {
          case 2:
            bitdepth *= 3;   /* RGB */
            break;
          case 4:
            bitdepth *= 2;   /* gray+alpha */
            break;
          case 6:
            bitdepth *= 4;   /* RGBA */
            break;
        }
        if (verbose && no_err(1)) {
          printf("\n    %ld x %ld image, %d-bit %s, %sinterlaced\n", w, h,
            bitdepth, (ityp > 6)? png_type[1]:png_type[ityp], lace? "":"non-");
        }
      }
      ihdr_read = 1;
      last_is_idat = last_is_jdat = 0;
#ifdef USE_ZLIB
      first_idat = 1;  /* flag:  next IDAT will be the first in this file */
      zlib_error = 0;  /* flag:  no zlib errors yet in this file */
      /* GRR 20000304:  data dump not yet compatible with interlaced images: */
      if (lace && verbose > 3)
        verbose = 2;
#endif

    /*------* 
     | JHDR | 
     *------*/
    } else if (strcmp(chunkid, "JHDR") == 0) {
      if (png) {
        printf("%s  JHDR not defined in PNG\n", verbose? ":":fname);
        set_err(1);
      } else if (jng && jhdr_read) {
        printf("%s  multiple JHDR not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (sz != 16) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"JHDR ");
        set_err(2);
      }
      if (no_err(1)) {
        w = LG(buffer);
        h = LG(buffer+4);
        if (w == 0 || h == 0) {
          printf("%s  invalid %simage dimensions (%ldx%ld)\n",
            verbose? ":":fname, verbose? "":"JHDR ", w, h);
          set_err(1);
        }
        jtyp = (uch)buffer[8];
        if (jtyp != 8 && jtyp != 10 && jtyp != 12 && jtyp != 14) {
          printf("%s  invalid %scolor type\n",
            verbose? ":":fname, verbose? "":"JHDR ");
          set_err(1);
        } else {
          jtyp = (jtyp >> 1) - 4;   /* now 0,1,2,3:  index into jng_type[] */
          bitdepth = (uch)buffer[9];
          if (bitdepth != 8 && bitdepth != 12 && bitdepth != 20) {
            printf("%s  invalid %sbit depth (%d)\n",
              verbose? ":":fname, verbose? "":"JHDR ", bitdepth);
            set_err(1);
          } else if (buffer[10] != 8) {
            printf("%s  invalid %sJPEG compression method (%d)\n",
              verbose? ":":fname, verbose? "":"JHDR ", buffer[10]);
            set_err(1);
          } else if (buffer[13] != 0) {
            printf("%s  invalid %salpha-channel compression method (%d)\n",
              verbose? ":":fname, verbose? "":"JHDR ", buffer[13]);
            set_err(1);
          } else if (buffer[14] != 0) {
            printf("%s  invalid %salpha-channel filter method (%d)\n",
              verbose? ":":fname, verbose? "":"JHDR ", buffer[14]);
            set_err(1);
          } else if (buffer[15] != 0) {
            printf("%s  invalid %salpha-channel interlace method (%d)\n",
              verbose? ":":fname, verbose? "":"JHDR ", buffer[15]);
            set_err(1);
          } else if ((lace = (uch)buffer[11]) != 0 && lace != 8) {
            printf("%s  invalid %sJPEG interlace method (%d)\n",
              verbose? ":":fname, verbose? "":"JHDR ", lace);
            set_err(1);
          } else {
            int a;

            if (bitdepth == 20) {
              need_jsep = 1;
              jbitd = 8;
              and = "and 12-bit ";
            } else
              jbitd = bitdepth;
            a = alphadepth = buffer[12];
            if ((a != 1 && a != 2 && a != 4 && a != 8 && a != 16 &&
                       jtyp > 1) || (a != 0 && jtyp < 2))
            {
              printf("%s  invalid %salpha-channel bit depth (%d) for %s image\n"
                , verbose? ":":fname, verbose? "":"JHDR ", alphadepth,
                jng_type[jtyp]);
              set_err(1);
            } else if (verbose && no_err(1)) {
              if (jtyp < 2)
                printf("\n    %ld x %ld image, %d-bit %s%s%s\n",
                  w, h, jbitd, and, jng_type[jtyp], lace? ", progressive":"");
              else
                printf("\n    %ld x %ld image, %d-bit %s%s + %d-bit alpha%s\n",
                  w, h, jbitd, and, jng_type[jtyp-2], alphadepth,
                  lace? ", progressive":"");
            }
          }
        }
      }
      jhdr_read = 1;
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | MHDR | 
     *------*/
    } else if (strcmp(chunkid, "MHDR") == 0) {
      if (png || jng) {
        printf("%s  MHDR not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else
      if (mhdr_read) {
        printf("%s  multiple MHDR not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (sz != 28) {
        printf("%s  incorrect %slength\n",
               verbose? ":":fname, verbose? "":"MHDR ");
        set_err(2);
      }
      if (no_err(1)) {
        ulg tps, playtime, profile;

        mng_width  = w = LG(buffer);
        mng_height = h = LG(buffer+4);
        tps = LG(buffer+8);
        layers = LG(buffer+12);
        frames = LG(buffer+16);
        playtime = LG(buffer+20);
        profile = LG(buffer+24);
        if (verbose) {
          printf("\n    %lu x %lu frame size, ", w, h);
          if (tps)
            printf("%lu tick%s per second, ", tps, (tps == 1L)? "" : "s");
          else
            printf("infinite tick length, ");
          if (layers && layers < 0x7ffffffL)
            printf("%lu layer%s,\n", layers, (layers == 1L)? "" : "s");
          else
            printf("%s layer count,\n", layers? "infinite" : "unspecified");
          if (frames && frames < 0x7ffffffL)
            printf("    %lu frame%s, ", frames, (frames == 1L)? "" : "s");
          else
            printf("    %s frame count, ", frames? "infinite" : "unspecified");
          if (playtime && playtime < 0x7ffffffL) {
            printf("%lu-tick play time ", playtime);
            if (tps)
              printf("(%lu seconds), ", (playtime + (tps >> 1)) / tps);
            else
              printf(", ");
          } else
            printf("%s play time, ", playtime? "infinite" : "unspecified");
        }
        if (profile & 1) {
          int bits = 0;

          vlc = lc = 1;
          if (verbose)
            printf("valid profile:\n      ");
          if (profile & 2) {
            if (verbose)
              printf("simple MNG features");
            ++bits;
            vlc = 0;
          }
          if (profile & 4) {
            if (verbose)
              printf("%scomplex MNG features", bits? ", " : "");
            ++bits;
            vlc = 0;
            lc = 0;
          }
          if (profile & 8) {
            if (verbose)
              printf("%scritical transparency", bits? ", " : "");
            ++bits;
          }
          if (profile & 16) {
            if (verbose)
              printf("%s%sJNG", bits? ", " : "", (bits == 3)? "\n      " : "");
            ++bits;
            vlc = 0;
            lc = 0;
          }
          if (profile & 32) {
            if (verbose)
              printf("%s%sdelta-PNG", bits? ", " : "",
                (bits == 3)? "\n      " : "");
            ++bits;
            vlc = 0;
            lc = 0;
          }
          if (verbose) {
            if (vlc)
              printf(" (MNG-VLC)");
            else if (lc)
              printf(" (MNG-LC)");
            printf("\n");
          }
        } else {
          vlc = lc = -1;
          if (verbose)
            printf("%s\n    simplicity profile\n",
              profile? "invalid" : "unspecified");
        }
      }
      mhdr_read = 1;
      last_is_idat = last_is_jdat = 0;

    /*================================================*
     * PNG chunks (with the exception of IHDR, above) *
     *================================================*/

    /*------* 
     | PLTE | 
     *------*/
    } else if (strcmp(chunkid, "PLTE") == 0) {
      if (jng) {
        printf("%s  PLTE not defined in JNG\n", verbose? ":":fname);
        set_err(1);
      } else if (png && plte_read) {
        printf("%s  multiple PLTE not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (png && idat_read) {
        printf("%s  %smust precede IDAT\n",
               verbose? ":":fname, verbose? "":"PLTE ");
        set_err(1);
      } else if (png && have_bkgd) {
        printf("%s  %smust precede bKGD\n",
               verbose? ":":fname, verbose? "":"PLTE ");
        set_err(1);
      } else if ((!(mng && plte_read) && sz < 3) || sz > 768 || sz % 3 != 0) {
        printf("%s  invalid number of %sentries (%g)\n",
               verbose? ":":fname, verbose? "":"PLTE ",(double)sz / 3);
        set_err(2);
      }
/*
      else if (printpal && !verbose)
        printf("\n");
 */
      if (no_err(1)) {
        nplte = sz / 3;
        if (verbose || (printpal && !quiet)) {
          if (printpal && !quiet)
            printf("  PLTE chunk");
          printf(": %d palette entr%s\n", nplte, nplte == 1? "y":"ies");
        }
        if (printpal) {
          char *spc;

          if (nplte < 10)
            spc = "  ";
          else if (nplte < 100)
            spc = "   ";
          else
            spc = "    ";
          for (i = j = 0; i < nplte; i++, j += 3)
            printf("%s%3d:  (%3d,%3d,%3d) = (0x%02x,0x%02x,0x%02x)\n", spc, i,
                   buffer[j], buffer[j + 1], buffer[j + 2],
                   buffer[j], buffer[j + 1], buffer[j + 2]);
        }
      }
      plte_read = 1;
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | IDAT | 
     *------*/
    } else if (strcmp(chunkid, "IDAT") == 0) {
      /* GRR:  need to check for consecutive IDATs within MNG segments */
      if (idat_read && !last_is_idat) {
        if (mng) {  /* reset things (GRR SEMI-HACK:  check for segments!) */
          idat_read = 0;
          zhead = 1;
          if (verbose)
            printf("\n");
        } else {
          printf("%s  IDAT chunks must be consecutive\n", verbose? ":":fname);
          set_err(2);
          if (!force)
            return;
        }
      } else if (verbose)
        printf("\n");

      /* We just want to check that we have read at least the minimum (10)
       * IDAT bytes possible, but avoid any overflow for short ints.  We
       * must also take into account that 0 length IDAT chunks are legal.
       */
      if (idat_read <= 0)
        idat_read = (sz > 0)? sz : -1;
      else if (idat_read < 10)
        idat_read += (sz > 10)? 10 : sz;

      /* Dump the zlib header from the first two bytes. */
      if (verbose && zhead < 0x10000 && sz > 0) {
        zhead = (zhead << 8) + buffer[0];
        if (sz > 1 && zhead < 0x10000)
          zhead = (zhead << 8) + buffer[1];
        if (zhead >= 0x10000)
          print_zlibheader(zhead & 0xffff);
      }
#ifdef USE_ZLIB
      if (verbose > 1 && !zlib_error) {
        static uch *p;   /* always points to next filter byte */
        static int cur_y, cur_pass, cur_xoff, cur_yoff, cur_xskip, cur_yskip;
        static long cur_width, cur_linebytes;
        static long numfilt, numfilt_this_block, numfilt_total;
        uch *eod;
        int err=Z_OK;

        zstrm.next_in = buffer;
        zstrm.avail_in = toread;

        /* initialize zlib and bit/byte/line variables if not already done */
        if (first_idat) {
          zstrm.next_out = p = outbuf;
          zstrm.avail_out = BS;
          zstrm.zalloc = (alloc_func)Z_NULL;
          zstrm.zfree = (free_func)Z_NULL;
          zstrm.opaque = (voidpf)Z_NULL;
          if ((err = inflateInit(&zstrm)) != Z_OK) {
            fprintf(stderr, "    zlib:  oops! can't initialize (error = %d)\n",
              err);
            verbose = 1;   /* this is a fatal error for all subsequent PNGs */
          }
          cur_y = 0;
          cur_pass = 1;    /* interlace pass:  1 through 7 */
          cur_xoff = cur_yoff = 0;
          cur_xskip = cur_yskip = lace? 8 : 1;
          cur_width = (w + cur_xskip - 1) / cur_xskip;         /* round up */
          cur_linebytes = ((cur_width*bitdepth + 7) >> 3) + 1; /* round, fltr */
          numfilt = 0L;
          first_idat = 0;
          if (lace) {   /* loop through passes to calculate total filters */
            int pass, yskip, yoff;

            numfilt_total = 0L;
            for (pass = 1;  pass <= 7;  ++pass) {
              switch (pass) {
                case 1:  /* fall through (see table below for full summary) */
                case 2:  yskip = 8; yoff = 0; break;
                case 3:  yskip = 8; yoff = 4; break;
                case 4:  yskip = 4; yoff = 0; break;
                case 5:  yskip = 4; yoff = 2; break;
                case 6:  yskip = 2; yoff = 0; break;
                case 7:  yskip = 2; yoff = 1; break;
              }
              /* effective height is reduced if odd pass: subtract yoff */
              numfilt_total += (h - yoff + yskip - 1) / yskip;
            }
          } else
            numfilt_total = h;   /* if non-interlaced */
        }

        printf("    zlib line filters (0 none, 1 sub, 2 up, 3 avg, 4 paeth)%s:"
          "\n     ", verbose > 3? " and data" : "");
        numfilt_this_block = 0L;

        while (err != Z_STREAM_END && zstrm.avail_in > 0) {
          /* know zstrm.avail_out > 0:  get some image/filter data */
          err = inflate(&zstrm, Z_PARTIAL_FLUSH);
          if (err != Z_OK && err != Z_STREAM_END) {
            fflush(stdout);
            fprintf(stderr, "\n    zlib:  inflate (first loop) error = %d\n",
              err);
            fflush(stderr);
            zlib_error = 1;      /* fatal error only for this PNG */
            break;               /* kill inner loop */
          }

          /* now have uncompressed, filtered image data in outbuf */
          eod = outbuf + BS - zstrm.avail_out;
          while (p < eod) {
            if (verbose > 3) {			/* GRR 20000304 */
              printf(" [%1d]", (int)p[0]);
              fflush(stdout);
              ++numfilt;
              for (i = 1;  i < cur_linebytes;  ++i, ++p) {
                printf(" %d", (int)p[1]);
                fflush(stdout);
              }
              ++p;
              printf("\n     ");
              fflush(stdout);
              cur_y += cur_yskip;
            } else {
              printf(" %1d", (int)p[0]);
              ++numfilt;
              if (++numfilt_this_block % 25 == 0)
                printf("\n     ");
              p += cur_linebytes;
              cur_y += cur_yskip;
            }
            if (lace) {
              while (cur_y >= h) {      /* may loop if very short image */
                /*
                          pass  xskip yskip  xoff yoff
                            1     8     8      0    0
                            2     8     8      4    0
                            3     4     8      0    4
                            4     4     4      2    0
                            5     2     4      0    2
                            6     2     2      1    0
                            7     1     2      0    1
                 */
                ++cur_pass;
                if (cur_pass & 1) {   /* beginning an odd pass */
                  cur_yoff = cur_xoff;
                  cur_xoff = 0;
                  cur_xskip >>= 1;
                } else {              /* beginning an even pass */
                  if (cur_pass == 2)
                    cur_xoff = 4;
                  else {
                    cur_xoff = cur_yoff >> 1;
                    cur_yskip >>= 1;
                  }
                  cur_yoff = 0;
                }
                cur_y = cur_yoff;
                /* effective width is reduced if even pass: subtract cur_xoff */
                cur_width = (w - cur_xoff + cur_xskip - 1) / cur_xskip;
                cur_linebytes = ((cur_width*bitdepth + 7) >> 3) + 1;
              }
            } else if (cur_y >= h) {
              if (verbose > 3) {		/* GRR 20000304:  bad code */
                printf(" %d bytes remaining in buffer before inflateEnd()",
                  eod-p);
                printf("\n     ");
                fflush(stdout);
                i = inflateEnd(&zstrm);     /* we're all done */
                if (i == Z_OK || i == Z_STREAM_ERROR)
                  printf(" inflateEnd() returns %s\n     ",
                    i == Z_OK? "Z_OK" : "Z_STREAM_ERROR");
                else
                  printf(" inflateEnd() returns %d\n     ", i);
                fflush(stdout);
              } else
                inflateEnd(&zstrm);   /* we're all done */
              zlib_error = 99;        /* kill outermost loop */
              err = Z_STREAM_END;     /* kill middle loop */
              break;                  /* kill innermost loop */
            }
          }
          p -= (eod - outbuf);        /* wrap p back into outbuf region */
          zstrm.next_out = outbuf;
          zstrm.avail_out = BS;

          /* get more input (waiting until buffer empties is not necessary best
           * zlib strategy, but simpler than shifting left-over data around) */
          if (zstrm.avail_in == 0 && sz > toread) {
            int data_read;

            sz -= toread;
            toread = (sz > BS)? BS:sz;
            if ((data_read = fread(buffer, 1, toread, fp)) != toread) {
              printf("\nEOF while reading %s data\n", chunkid);
              set_err(3);
              return;
            }
            crc = update_crc(crc, buffer, toread);
            zstrm.next_in = buffer;
            zstrm.avail_in = toread;
          }
        }
        printf(" (%ld out of %ld)\n", numfilt, numfilt_total);
      }
#endif /* USE_ZLIB */
      last_is_idat = 1;
      last_is_jdat = 0;

    /*------* 
     | IEND | 
     *------*/
    } else if (strcmp(chunkid, "IEND") == 0) {
      if (!mng && iend_read) {
        printf("%s  multiple IEND not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (sz != 0) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"IEND ");
        set_err(1);
      } else if (jng && need_jsep && !jsep_read) {
        printf("%s  missing JSEP in 20-bit JNG\n", verbose? ":":fname);
        set_err(1);
      } else if (png && idat_read < 10) {
        printf("%s  not enough IDAT data\n", verbose? ":":fname);
      } else if (verbose) {
        printf("\n");
      }
      iend_read = 1;
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | bKGD | 
     *------*/
    } else if (strcmp(chunkid, "bKGD") == 0) {
      if (!mng && have_bkgd) {
        printf("%s  multiple bKGD not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (!mng && (idat_read || jdat_read)) {
        printf("%s  %smust precede IDAT\n",
          verbose? ":":fname, verbose? "":"bKGD ");
        set_err(1);
      }
      switch (ityp) {
        case 0:
        case 4:
          if (sz != 2) {
            printf("%s  incorrect %slength\n",
              verbose? ":":fname, verbose? "":"bKGD ");
            set_err(2);
          }
          if (verbose && no_err(1)) {
            printf(": gray = %d\n", SH(buffer));
          }
          break;
        case 2:
        case 6:
          if (sz != 6) {
            printf("%s  incorrect %slength\n",
              verbose? ":":fname, verbose? "":"bKGD ");
            set_err(2);
          }
          if (verbose && no_err(1)) {
            printf(": red = %d green = %d blue = %d\n", SH(buffer),
              SH(buffer+2), SH(buffer+4));
          }
          break;
        case 3:
          if (sz != 1) {
            printf("%s  incorrect %slength\n",
              verbose? ":":fname, verbose? "":"bKGD ");
            set_err(2);
          }
          if (verbose && no_err(1)) {
            printf(": index = %d\n", buffer[0]);
          }
          break;
      }
      have_bkgd = 1;
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | cHRM | 
     *------*/
    } else if (strcmp(chunkid, "cHRM") == 0) {
      if (!mng && have_chrm) {
        printf("%s  multiple cHRM not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (!mng && plte_read) {
        printf("%s  %smust precede PLTE\n",
               verbose? ":":fname, verbose? "":"cHRM ");
        set_err(1);
      } else if (!mng && (idat_read || jdat_read)) {
        printf("%s  %smust precede IDAT\n",
               verbose? ":":fname, verbose? "":"cHRM ");
        set_err(1);
      } else if (sz != 32) {
        printf("%s  incorrect %slength\n",
               verbose? ":":fname, verbose? "":"cHRM ");
        set_err(2);
      }
      if (no_err(1)) {
        double wx, wy, rx, ry, gx, gy, bx, by;

        wx = (double)LG(buffer)/100000;
        wy = (double)LG(buffer+4)/100000;
        rx = (double)LG(buffer+8)/100000;
        ry = (double)LG(buffer+12)/100000;
        gx = (double)LG(buffer+16)/100000;
        gy = (double)LG(buffer+20)/100000;
        bx = (double)LG(buffer+24)/100000;
        by = (double)LG(buffer+28)/100000;

        if (wx < 0 || wx > 0.8 || wy < 0 || wy > 0.8 || wx + wy > 1.0) {
          printf("%s  invalid %swhite point %0g %0g\n",
                 verbose? ":":fname, verbose? "":"cHRM ", wx, wy);
          set_err(1);
        } else if (rx < 0 || rx > 0.8 || ry < 0 || ry > 0.8 || rx + ry > 1.0) {
          printf("%s  invalid %sred point %0g %0g\n",
                 verbose? ":":fname, verbose? "":"cHRM ", rx, ry);
          set_err(1);
        } else if (gx < 0 || gx > 0.8 || gy < 0 || gy > 0.8 || gx + gy > 1.0) {
          printf("%s  invalid %sgreen point %0g %0g\n",
                 verbose? ":":fname, verbose? "":"cHRM ", gx, gy);
          set_err(1);
        } else if (bx < 0 || bx > 0.8 || by < 0 || by > 0.8 || bx + by > 1.0) {
          printf("%s  invalid %sblue point %0g %0g\n",
                 verbose? ":":fname, verbose? "":"cHRM ", bx, by);
          set_err(1);
        }
        else if (verbose) {
          printf("\n");
        }

        if (verbose && no_err(1)) {
          printf("    White x = %0g y = %0g,  Red x = %0g y = %0g\n",
                 wx, wy, rx, ry);
          printf("    Green x = %0g y = %0g,  Blue x = %0g y = %0g\n",
                 gx, gy, bx, by);
        }
      }
      have_chrm = 1;
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | fRAc | 
     *------*/
    } else if (strcmp(chunkid, "fRAc") == 0) {
      if (verbose)
        printf("\n    undefined fractal parameters (ancillary, safe to copy)\n"
          "    [contact Tim Wegner, twegner@phoenix.net, for specification]\n");
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | gAMA | 
     *------*/
    } else if (strcmp(chunkid, "gAMA") == 0) {
      if (!mng && have_gama) {
        printf("%s  multiple gAMA not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (!mng && (idat_read || jdat_read)) {
        printf("%s  %smust precede IDAT\n",
               verbose? ":":fname, verbose? "":"gAMA ");
        set_err(1);
      } else if (!mng && plte_read) {
        printf("%s  %smust precede PLTE\n",
               verbose? ":":fname, verbose? "":"gAMA ");
        set_err(1);
      } else if (sz != 4) {
        printf("%s  incorrect %slength\n",
                     verbose? ":":fname, verbose? "":"gAMA ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        printf(": %#0.5g\n", (double)LG(buffer)/100000);
      }
      have_gama = 1;
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | gIFg | 
     *------*/
    } else if (strcmp(chunkid, "gIFg") == 0) {
      if (jng) {
        printf("%s  gIFg not defined in JNG\n", verbose? ":":fname);
        set_err(1);
      } else if (sz != 4) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"gIFg ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        float dtime = .01 * SH(buffer+2);

        printf("\n    disposal method = %d, user input flag = %d, display time = %f seconds\n",
          buffer[0], buffer[1], dtime);
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | gIFt | 
     *------*/
    } else if (strcmp(chunkid, "gIFt") == 0) {
      printf("%s  %sDEPRECATED CHUNK\n",
        verbose? ":":fname, verbose? "":"gIFt ");
      set_err(1);
      if (jng) {
        printf("%s  gIFt not defined in JNG\n", verbose? ":":fname);
        set_err(1);
      } else if (sz < 24) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"gIFt ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        printf("    %ldx%ld-pixel text grid at (%ld,%ld) pixels from upper "
          "left\n", LG(buffer+8), LG(buffer+12), LG(buffer), LG(buffer+4));
        printf("    character cell = %dx%d pixels\n", buffer[16], buffer[17]);
        printf("    foreground color = 0x%02d%02d%02d, background color = "
          "0x%02d%02d%02d\n", buffer[18], buffer[19], buffer[20], buffer[21],
          buffer[22], buffer[23]);
        printf("    %ld bytes of text data\n", sz-24);
        /* GRR:  print text according to grid size/cell size? */
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | gIFx | 
     *------*/
    } else if (strcmp(chunkid, "gIFx") == 0) {
      if (jng) {
        printf("%s  gIFx not defined in JNG\n", verbose? ":":fname);
        set_err(1);
      } else if (sz < 11) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"gIFx ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        printf(
          "\n    application ID = %.*s, authentication code = 0x%02x%02x%02x\n",
          8, buffer, buffer[8], buffer[9], buffer[10]);
        printf("    %ld bytes of application data\n", sz-11);
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | hIST | 
     *------*/
    } else if (strcmp(chunkid, "hIST") == 0) {
      if (jng) {
        printf("%s  hIST not defined in JNG\n", verbose? ":":fname);
        set_err(1);
      } else if (png && have_hist) {
        printf("%s  multiple hIST not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (!plte_read) {
        printf("%s  %smust follow PLTE\n",
               verbose? ":":fname, verbose? "":"hIST ");
        set_err(1);
      } else if (png && idat_read) {
        printf("%s  %smust precede IDAT\n",
               verbose? ":":fname, verbose? "":"hIST ");
        set_err(1);
      } else if (sz != nplte * 2) {
        printf("%s  invalid number of %sentries (%g)\n",
               verbose? ":":fname, verbose? "":"hIST ", (double)sz / 2);
        set_err(2);
      }
      if ((verbose || (printpal && !quiet)) && no_err(1)) {
        if (printpal && !quiet)
          printf("  hIST chunk");
        printf(": %ld histogram entr%s\n", sz / 2, sz/2 == 1? "y":"ies");
      }
      if (printpal && no_err(1)) {
        char *spc;

        if (sz < 10)
          spc = "  ";
        else if (sz < 100)
          spc = "   ";
        else
          spc = "    ";
        for (i = j = 0;  j < sz;  ++i, j += 2)
          printf("%s%3d:  %5u\n", spc, i, SH(buffer+j));
      }
      have_hist = 1;
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | iCCP | 
     *------*/
    } else if (strcmp(chunkid, "iCCP") == 0) {
      if (!mng && have_iccp) {
        printf("%s  multiple iCCP not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (!mng && have_srgb) {
        printf("%s  %snot allowed with sRGB\n",
               verbose? ":":fname, verbose? "":"iCCP ");
        set_err(1);
      } else if (!mng && plte_read) {
        printf("%s  %smust precede PLTE\n",
               verbose? ":":fname, verbose? "":"iCCP ");
        set_err(1);
      } else if (!mng && (idat_read || jdat_read)) {
        printf("%s  %smust precede IDAT\n",
               verbose? ":":fname, verbose? "":"iCCP ");
        set_err(1);
      }
      if (no_err(1)) {
        uch *p=buffer, meth;
        int name_len;
        long remainder;

        while (*p && p < buffer + sz)
          ++p;
        name_len = p - buffer;
        if (name_len == 0) {
          printf("%s  zero length %sprofile name\n",
            verbose? ":":fname, verbose? "":"iCCP ");
          set_err(1);
        } else if (name_len > 79) {
          printf("%s  %s profile name longer than 79 characters\n",
            verbose? ":":fname, verbose? "":chunkid);
          set_err(1);
        } else if ((remainder = sz - name_len - 3) < 0L) {
          printf("%s  incorrect %slength\n",
            verbose? ":":fname, verbose? "":"iCCP ");
          set_err(2);
        } else if (buffer[name_len] != 0) {
          printf("%s  missing NULL after %sprofile name\n",
            verbose? ":":fname, verbose? "":"iCCP ");
          set_err(2);
        } else if ((meth = buffer[name_len+1]) > 0 && meth < 128) {
          printf("%s  invalid %scompression method (%d)\n",
            verbose? ":":fname, verbose? "":"sPLT ", meth);
          set_err(1);
        } else if (verbose && no_err(1)) {
          printf("\n    profile name = ");
          init_printbuffer(fname);
          printbuffer(buffer, name_len, 0);
          finish_printbuffer(fname, chunkid);
          printf("%scompression method = %d (%s)%scompressed profile = "
            "%ld bytes\n", (name_len > 24)? "\n    ":", ", meth,
            (meth == 0)? "deflate":"private", (name_len > 24)? ", ":"\n    ",
            sz-name_len-2);
        }
      }
      have_iccp = 1;
      last_is_idat = last_is_jdat = 0;

    /*------*
     | iTXt |
     *------*/
    } else if (strcmp(chunkid, "iTXt") == 0) {
      int key_len;

      key_len = keywordlen(buffer, toread);
      if (key_len == 0) {
        printf("%s  zero length %s%skeyword\n",
               verbose? ":":fname, verbose? "":chunkid, verbose? "":" ");
        set_err(1);
      } else if (key_len > 79) {
        printf("%s  %s keyword longer than 79 characters\n",
               verbose? ":":fname, verbose? "":chunkid);
        set_err(1);
      }
      if (no_err(1) && buffer[0] == ' ') {
        printf("%s  %s keyword has leading space(s)\n",
               verbose? ":":fname, verbose? "":chunkid);
        set_err(1);
      }
      if (no_err(1) && buffer[key_len - 1] == ' ') {
        printf("%s  %s keyword has trailing space(s)\n",
               verbose? ":":fname, verbose? "":chunkid);
        set_err(1);
      }
      if (no_err(1)) {
        int prev_space = 0;

        for(i = 0; i < key_len; i++) {
          if (buffer[i] == ' ') {
            if (prev_space) {
              printf("%s  %s keyword has consecutive spaces\n",
                     verbose? ":":fname, verbose? "":chunkid);
              set_err(1);
              break;
            } else {
              prev_space = 1;
            }
          } else {
            prev_space = 0;
          }
        }
      }
      if (no_err(1)) {
        for(i = 0; i < key_len; i++) {
          if ((uch)buffer[i]<32 ||
             ((uch)buffer[i]>126 && (uch)buffer[i]<161)) {
            printf("%s  %s keyword has control characters\n",
                   verbose? ":":fname, verbose? "":chunkid);
            set_err(1);
            break;
          }
        }
      }
      if (no_err(1)) {
        int compressed = 0, tag_len = 0;

        init_printbuffer(fname);
        if (verbose) {
          printf(", keyword: ");
        }
        if (verbose || printtext) {
          printbuffer(buffer, key_len, 0);
        }
        if (verbose)
          printf("\n");
        else if (printtext)
          printf(":\n");
        compressed = buffer[key_len+1];
        if (compressed < 0 || compressed > 1) {
          printf("%s  invalid %scompression flag (%d)\n",
            verbose? ":":fname, verbose? "":"iTXt ", compressed);
          set_err(1);
        } else if (buffer[key_len+2] != 0) {
          printf("%s  invalid %scompression method (%d)\n",
            verbose? ":":fname, verbose? "":"iTXt ", buffer[key_len+2]);
          set_err(1);
        }
        if (no_err(1)) {
          tag_len = keywordlen(buffer+key_len+3, toread-key_len-3);
          if (verbose) {
            if (tag_len > 0) {
              printf("    %scompressed, language tag = ", compressed? "":"un");
              printbuffer(buffer+key_len+3, tag_len, 0);
            } else {
              printf("    %scompressed, no language tag",
                compressed? "":"un");
            }
            if (buffer[key_len+3+tag_len+1] == 0)
              printf("\n    no translated keyword, %ld bytes of UTF-8 text\n",
                sz - (key_len+3+tag_len+1));
            else
              printf("\n    %ld bytes of translated keyword and UTF-8 text\n",
                sz - (key_len+3+tag_len));
          } else if (printtext) {
            if (buffer[key_len+3+tag_len+1] == 0)
              printf("    (no translated keyword, %ld bytes of UTF-8 text)\n",
                sz - (key_len+3+tag_len+1));
            else
              printf("    (%ld bytes of translated keyword and UTF-8 text)\n",
                sz - (key_len+3+tag_len));
          }
        }
        finish_printbuffer(fname, chunkid);   /* print CR/LF & NULLs info */
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | oFFs | 
     *------*/
    } else if (strcmp(chunkid, "oFFs") == 0) {
      if (!mng && have_offs) {
        printf("%s  multiple oFFs not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (!mng && (idat_read || jdat_read)) {
        printf("%s  %smust precede IDAT\n",
               verbose? ":":fname, verbose? "":"oFFs ");
        set_err(1);
      } else if (sz != 9) {
        printf("%s  incorrect %slength\n",
               verbose? ":":fname, verbose? "":"oFFs ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        printf(": %ldx%ld %s offset\n", LG(buffer), LG(buffer+4),
               buffer[8]==0 ? "pixels":
               buffer[8]==1 ? "micrometers":"unknown unit");
      }
      have_offs = 1;
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | pCAL | 
     *------*/
    } else if (strcmp(chunkid, "pCAL") == 0) {
      if (jng) {
        printf("%s  pCAL not defined in JNG\n", verbose? ":":fname);
        set_err(1);
      } else if (png && have_pcal) {
        printf("%s  multiple pCAL not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (png && idat_read) {
        printf("%s  %smust precede IDAT\n",
          verbose? ":":fname, verbose? "":"pCAL ");
        set_err(1);
      }
      if (no_err(1)) {
        int name_len = keywordlen(buffer, toread);

        if (name_len == 0) {
          printf("%s  zero length %s%scalibration name\n",
            verbose? ":":fname, verbose? "":chunkid, verbose? "":" ");
          set_err(1);
        } else if (name_len > 79) {
          printf("%s  %s calibration name longer than 79 characters\n",
            verbose? ":":fname, verbose? "":chunkid);
          set_err(1);
        } else if (sz < name_len + 15) {
          printf("%s  incorrect %slength\n",
            verbose? ":":fname, verbose? "":"pCAL ");
          set_err(2);
        } else if (buffer[0] == ' ') {
          printf("%s  %s calibration name has leading space(s)\n",
            verbose? ":":fname, verbose? "":chunkid);
          set_err(1);
        } else if (buffer[name_len - 1] == ' ') {
          printf("%s  %s calibration name has trailing space(s)\n",
            verbose? ":":fname, verbose? "":chunkid);
          set_err(1);
        } else {
          int prev_space = 0;

          for(i = 0; i < name_len; i++) {
            if (buffer[i] == ' ') {
              if (prev_space) {
                printf("%s  %s calibration name has consecutive spaces\n",
                  verbose? ":":fname, verbose? "":chunkid);
                set_err(1);
                break;
              } else {
                prev_space = 1;
              }
            } else {
              prev_space = 0;
            }
          }
        }
        if (no_err(1)) {
          for(i = 0; i < name_len; i++) {
            if ((uch)buffer[i]<32 ||
               ((uch)buffer[i]>126 && (uch)buffer[i]<161)) {
              printf("%s  %s calibration name has control characters\n",
                verbose? ":":fname, verbose? "":chunkid);
              set_err(1);
              break;
            }
          }
        }
        if (no_err(1)) {
          long x0 = LG(buffer+name_len+1);	/* already checked sz above */
          long x1 = LG(buffer+name_len+5);
          int eqn_num = buffer[name_len+9];
          int num_params = buffer[name_len+10];

          if (eqn_num < 0 || eqn_num > 3) {
            printf("%s  invalid %s equation type (%d)\n",
              verbose? ":":fname, verbose? "":chunkid, eqn_num);
            set_err(1);
          } else if (num_params != eqn_params[eqn_num]) {
            printf(
              "%s  invalid number of parameters (%d) for %s equation type %d\n",
              verbose? ":":fname, num_params, verbose? "":chunkid, eqn_num);
            set_err(1);
          } else if (verbose) {
            long remainder;
            uch *pbuf;

            printf(", equation type %d\n", eqn_num);
            printf("    %s\n", eqn_type[eqn_num]);
            printf("    calibration name = ");
            init_printbuffer(fname);
            printbuffer(buffer, name_len, 0);
            finish_printbuffer(fname, chunkid);
            if (toread != sz) {
              printf(
                "\n    pngcheck INTERNAL LOGIC ERROR:  toread (%ld) != sz (%ld)"
                , toread, sz);
            } else
              remainder = toread - name_len - 11;
            pbuf = buffer + name_len + 11;
            if (*pbuf == 0)
              printf("\n    no physical_value unit name\n");
            else {
              int unit_len = keywordlen(pbuf, remainder);

              printf("\n    physical_value unit name = ");
              init_printbuffer(fname);
              printbuffer(pbuf, unit_len, 0);
              finish_printbuffer(fname, chunkid);
              printf("\n");
              pbuf += unit_len;
              remainder -= unit_len;
            }
            printf("    x0 = %ld\n", x0);
            printf("    x1 = %ld\n", x1);
            for (i = 0;  i < num_params;  ++i) {
              int len;

              if (remainder < 2) {
                printf("%s  incorrect %slength\n",
                  verbose? ":":fname, verbose? "":"pCAL ");
                set_err(2);
                break;
              }
              if (*pbuf != 0) {
                printf("%s  %smissing NULL separator\n",
                  verbose? ":":fname, verbose? "":"pCAL ");
                set_err(1);
                break;
              }
              ++pbuf;
              --remainder;
              len = keywordlen(pbuf, remainder);
              printf("    p%d = ", i);
              init_printbuffer(fname);
              printbuffer(pbuf, len, 0);
              finish_printbuffer(fname, chunkid);
              printf("\n");
              pbuf += len;
              remainder -= len;
            }
          }
        }
      }
      have_pcal = 1;
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | pHYs | 
     *------*/
    } else if (strcmp(chunkid, "pHYs") == 0) {
      if (!mng && have_phys) {
        printf("%s  multiple pHYS not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (!mng && (idat_read || jdat_read)) {
        printf("%s  %smust precede IDAT\n",
               verbose? ":":fname, verbose? "":"pHYS ");
        set_err(1);
      } else if (sz != 9) {
        printf("%s  incorrect %slength\n",
               verbose? ":":fname, verbose? "":"pHYS ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        long xres = LG(buffer);
        long yres = LG(buffer+4);

        printf(": %ldx%ld pixels/%s", xres, yres,
               buffer[8]==0 ? "unit":buffer[8]==1 ? "meter":"unknown unit");
        if (buffer[8] == 1 && xres == yres)
          printf(" (%ld dpi)", (long)(xres*0.0254 + 0.5));
        else if (buffer[8] > 1)
          set_err(1);
        printf("\n");
      }
      have_phys = 1;
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | sBIT | 
     *------*/
    } else if (strcmp(chunkid, "sBIT") == 0) {
      int maxbits = (ityp == 3)? 8 : bitdepth;

      if (jng) {
        printf("%s  sBIT not defined in JNG\n", verbose? ":":fname);
        set_err(1);
      } else if (png && have_sbit) {
        printf("%s  multiple sBIT not allowed\n", verbose? ":" : fname);
        set_err(1);
      } else if (plte_read) {
        printf("%s  %smust precede PLTE\n",
          verbose? ":" : fname, verbose? "" : "sBIT ");
        set_err(1);
      } else if (png && idat_read) {
        printf("%s  %smust precede IDAT\n",
          verbose? ":" : fname, verbose? "" : "sBIT ");
        set_err(1);
      }
      switch (ityp) {
        case 0:
          if (sz != 1) {
            printf("%s  incorrect %slength\n",
                   verbose? ":" : fname, verbose? "" : "sBIT ");
            set_err(2);
          } else if (buffer[0] == 0 || buffer[0] > maxbits) {
            printf("%s  %d %sgrey bits not valid for %dbit/sample image\n",
              verbose? ":" : fname, buffer[0], verbose? "" : "sBIT ", maxbits);
            set_err(1);
          } else if (verbose && no_err(1)) {
            printf(": gray = %d\n", buffer[0]);
          }
          break;
        case 2:
        case 3:
          if (sz != 3) {
            printf("%s  incorrect %slength\n",
                   verbose? ":":fname, verbose? "":"sBIT ");
            set_err(2);
          } else if (buffer[0] == 0 || buffer[0] > maxbits) {
            printf("%s  %d %sred bits not valid for %dbit/sample image\n",
                   verbose? ":":fname, buffer[0], verbose? "":"sBIT ", maxbits);
            set_err(1);
          } else if (buffer[1] == 0 || buffer[1] > maxbits) {
            printf("%s  %d %sgreen bits not valid for %dbit/sample image\n",
                   verbose? ":":fname, buffer[1], verbose? "":"sBIT ", maxbits);
            set_err(1);
          } else if (buffer[2] == 0 || buffer[2] > maxbits) {
            printf("%s  %d %sblue bits not valid for %dbit/sample image\n",
                   verbose? ":":fname, buffer[2], verbose? "":"sBIT ", maxbits);
            set_err(1);
          } else if (verbose && no_err(1)) {
            printf(": red = %d green = %d blue = %d\n",
                   buffer[0], buffer[1], buffer[2]);
          }
          break;
        case 4:
          if (sz != 2) {
            printf("%s  incorrect %slength\n",
                   verbose? ":":fname, verbose? "":"sBIT ");
            set_err(2);
          } else if (buffer[0] == 0 || buffer[0] > maxbits) {
            printf("%s  %d %sgrey bits not valid for %dbit/sample image\n",
                   verbose? ":":fname, buffer[0], verbose? "":"sBIT ", maxbits);
            set_err(2);
          } else if (buffer[1] == 0 || buffer[1] > maxbits) {
            printf("%s  %d %salpha bits(tm) not valid for %dbit/sample image\n",
                   verbose? ":":fname, buffer[1], verbose? "":"sBIT ", maxbits);
            set_err(2);
          } else if (verbose && no_err(1)) {
            printf(": gray = %d alpha = %d\n", buffer[0], buffer[1]);
          }
          break;
        case 6:
          if (sz != 4) {
            printf("%s  incorrect %slength\n",
                   verbose? ":":fname, verbose? "":"sBIT ");
            set_err(2);
          } else if (buffer[0] == 0 || buffer[0] > maxbits) {
            printf("%s  %d %sred bits not valid for %dbit/sample image\n",
                   verbose? ":":fname, buffer[0], verbose? "":"sBIT ", maxbits);
            set_err(1);
          } else if (buffer[1] == 0 || buffer[1] > maxbits) {
            printf("%s  %d %sgreen bits not valid for %dbit/sample image\n",
                   verbose? ":":fname, buffer[1], verbose? "":"sBIT ", maxbits);
            set_err(1);
          } else if (buffer[2] == 0 || buffer[2] > maxbits) {
            printf("%s  %d %sblue bits not valid for %dbit/sample image\n",
                   verbose? ":":fname, buffer[2], verbose? "":"sBIT ", maxbits);
            set_err(1);
          } else if (buffer[3] == 0 || buffer[3] > maxbits) {
            printf("%s  %d %salpha bits(tm) not valid for %dbit/sample image\n",
                   verbose? ":":fname, buffer[3], verbose? "":"sBIT ", maxbits);
            set_err(1);
          } else if (verbose && no_err(1)) {
            printf(": red = %d green = %d blue = %d alpha = %d\n",
                   buffer[0], buffer[1], buffer[2], buffer[3]);
          }
          break;
      }
      have_sbit = 1;
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | sCAL | 
     *------*/
    } else if (strcmp(chunkid, "sCAL") == 0) {
      if (!mng && have_scal) {
        printf("%s  multiple sCAL not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (!mng && (idat_read || jdat_read)) {
        printf("%s  %smust precede IDAT\n",
               verbose? ":":fname, verbose? "":"sCAL ");
        set_err(1);
      }
      if (verbose && no_err(1)) {
        buffer[sz] = '\0';
        buffer[strlen(buffer+1)+1] = 'x';

        printf(": image size %s %s\n", buffer+1,
               buffer[0] == 1 ? "meters":
               buffer[0] == 2 ? "radians":"unknown unit");
      }
      have_scal = 1;
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | sPLT | 
     *------*/
    } else if (strcmp(chunkid, "sPLT") == 0) {
      if (jng) {
        printf("%s  sPLT not defined in JNG\n", verbose? ":":fname);
        set_err(1);
      } else if (png && idat_read) {
        printf("%s  %smust precede IDAT\n",
          verbose? ":":fname, verbose? "":"sPLT ");
        set_err(1);
      } else {
        uch *p=buffer, bps;
        int name_len;
        long remainder;

        while (*p && p < buffer + sz)
          ++p;
        name_len = p - buffer;
        if (name_len == 0) {
          printf("%s  zero length %spalette name\n",
            verbose? ":":fname, verbose? "":"sPLT ");
          set_err(1);
        } else if (name_len > 79) {
          printf("%s  %s palette name longer than 79 characters\n",
            verbose? ":":fname, verbose? "":chunkid);
          set_err(1);
        } else if ((remainder = sz - name_len - 2) < 0L) {
          printf("%s  incorrect %slength\n",
            verbose? ":":fname, verbose? "":"sPLT ");
          set_err(2);
        } else if (buffer[name_len] != 0) {
          printf("%s  missing NULL after %spalette name\n",
            verbose? ":":fname, verbose? "":"sPLT ");
          set_err(2);
        } else if ((bps = buffer[name_len+1]) != 8 && bps != 16) {
          printf("%s  invalid %ssample depth (%u bits)\n",
            verbose? ":":fname, verbose? "":"sPLT ", bps);
          set_err(2);
        } else {
          int bytes = (bps >> 3);
          int entry_sz = 4*bytes + 2;
          long nsplt = remainder / entry_sz;

          if (remainder % entry_sz != 0) {
            printf("%s  invalid number of %sentries (%g)\n",
              verbose? ":":fname, verbose? "":"sPLT ",
              (double)remainder / entry_sz);
            set_err(2);
          }
          if ((verbose || (printpal && !quiet)) && no_err(1)) {
            if (printpal && !quiet)
              printf("  sPLT chunk");
            printf(": %ld palette/histogram entr%s\n", nsplt,
              nsplt == 1? "y":"ies");
            printf("    sample depth = %u bits, palette name = ", bps);
            init_printbuffer(fname);
            printbuffer(buffer, name_len, 0);
            finish_printbuffer(fname, chunkid);
            printf("\n");
          }
          if (printpal && no_err(1)) {
            char *spc;
            long i, j=name_len+2L;

            if (nsplt < 10L)
              spc = "  ";
            else if (nsplt < 100L)
              spc = "   ";
            else if (nsplt < 1000L)
              spc = "    ";
            else if (nsplt < 10000L)
              spc = "     ";
            else
              spc = "      ";
            /* GRR:  could check for (required) non-increasing freq order */
            /* GRR:  could also check for all zero freqs:  undefined hist */
            if (bytes == 1) {
              for (i = 0L;  i < nsplt;  ++i, j += 6L)
                printf("%s%3ld:  (%3u,%3u,%3u,%3u) = "
                  "(0x%02x,0x%02x,0x%02x,0x%02x)  freq = %u\n", spc, i,
                  buffer[j], buffer[j+1], buffer[j+2], buffer[j+3],
                  buffer[j], buffer[j+1], buffer[j+2], buffer[j+3],
                  SH(buffer+j+4));
            } else {
              for (i = 0L;  i < nsplt;  ++i, j += 10L)
                printf("%s%5ld:  (%5u,%5u,%5u,%5u) = (%04x,%04x,%04x,%04x)  "
                  "freq = %u\n", spc, i, SH(buffer+j), SH(buffer+j+2),
                  SH(buffer+j+4), SH(buffer+j+6), SH(buffer+j), SH(buffer+j+2),
                  SH(buffer+j+4), SH(buffer+j+6), SH(buffer+j+8));
            }
          }
        }
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | sRGB | 
     *------*/
    } else if (strcmp(chunkid, "sRGB") == 0) {
      if (!mng && have_srgb) {
        printf("%s  multiple sRGB not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (!mng && have_iccp) {
        printf("%s  %snot allowed with iCCP\n",
               verbose? ":":fname, verbose? "":"sRGB ");
        set_err(1);
      } else if (!mng && plte_read) {
        printf("%s  %smust precede PLTE\n",
               verbose? ":":fname, verbose? "":"sRGB ");
        set_err(1);
      } else if (!mng && (idat_read || jdat_read)) {
        printf("%s  %smust precede IDAT\n",
               verbose? ":":fname, verbose? "":"sRGB ");
        set_err(1);
      } else if (buffer[0] > 3) {
        printf("%s  %sinvalid rendering intent\n",
               verbose? ":":fname, verbose? "":"sRGB ");
        set_err(1);
      }
      if (verbose && no_err(1)) {
        printf("\n    rendering intent = %s\n", rendering_intent[buffer[0]]);
      }
      have_srgb = 1;
      last_is_idat = last_is_jdat = 0;

    /*------*  *------* 
     | tEXt |  | zTXt | 
     *------*  *------*/
    } else if (strcmp(chunkid, "tEXt") == 0 || strcmp(chunkid, "zTXt") == 0) {
      int key_len;

      key_len = keywordlen(buffer, toread);
      if (key_len == 0) {
        printf("%s  zero length %s%skeyword\n",
               verbose? ":":fname, verbose? "":chunkid, verbose? "":" ");
        set_err(1);
      } else if (key_len > 79) {
        printf("%s  %s keyword longer than 79 characters\n",
               verbose? ":":fname, verbose? "":chunkid);
        set_err(1);
      }
      if (no_err(1) && buffer[0] == ' ') {
        printf("%s  %s keyword has leading space(s)\n",
               verbose? ":":fname, verbose? "":chunkid);
        set_err(1);
      }
      if (no_err(1) && buffer[key_len - 1] == ' ') {
        printf("%s  %s keyword has trailing space(s)\n",
               verbose? ":":fname, verbose? "":chunkid);
        set_err(1);
      }
      if (no_err(1)) {
        int prev_space = 0;

        for(i = 0; i < key_len; i++) {
          if (buffer[i] == ' ') {
            if (prev_space) {
              printf("%s  %s keyword has consecutive spaces\n",
                     verbose? ":":fname, verbose? "":chunkid);
              set_err(1);
              break;
            } else {
              prev_space = 1;
            }
          } else {
            prev_space = 0;
          }
        }
      }
      if (no_err(1)) {
        for(i = 0; i < key_len; i++) {
          if ((uch)buffer[i]<32 ||
             ((uch)buffer[i]>=127 && (uch)buffer[i]<161)) {
            printf("%s  %s keyword has control characters\n",
                   verbose? ":":fname, verbose? "":chunkid);
            set_err(1);
            break;
          }
        }
      }
      if (no_err(1)) {
        init_printbuffer(fname);
        if (verbose) {
          printf(", keyword: ");
        }
        if (verbose || printtext) {
          printbuffer(buffer, key_len, 0);
        }
        if (printtext) {
          printf(verbose? "\n" : ":\n");
          if (strcmp(chunkid, "tEXt") == 0)
            printbuffer(buffer + key_len + 1, toread - key_len - 1, 1);
          else
            printf("%s(compressed %s text)", verbose? "    " : "", chunkid);

          /* For the sake of simplifying this program, we will not print
           * the contents of a tEXt chunk whose size is larger than the
           * buffer size (currently 32K).  People should use zTXt for
           * such large amounts of text, anyway!  Note that this does not
           * mean that the tEXt/zTXt contents will be lost if extracting.
           */
          printf("\n");
        } else if (verbose) {
          printf("\n");
        }
        finish_printbuffer(fname, chunkid);
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | tIME | 
     *------*/
    } else if (strcmp(chunkid, "tIME") == 0) {
      if (!mng && have_time) {
        printf("%s  multiple tIME not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (sz != 7) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"tIME ");
        set_err(2);
      }
      /* Print the date in RFC 1123 format, rather than stored order */
      if (verbose && no_err(1)) {
        printf(": %2d %s %4d %02d:%02d:%02d GMT\n", buffer[3],
          getmonth(buffer[2]), SH(buffer), buffer[4], buffer[5], buffer[6]);
      }
      have_time = 1;
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | tRNS | 
     *------*/
    } else if (strcmp(chunkid, "tRNS") == 0) {
      if (jng) {
        printf("%s  tRNS not defined in JNG\n", verbose? ":":fname);
        set_err(1);
      } else if (png && have_trns) {
        printf("%s  multiple tRNS not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (ityp == 3 && !plte_read) {
        printf("%s  %smust follow PLTE\n",
          verbose? ":":fname, verbose? "":"tRNS ");
        set_err(1);
      } else if (png && idat_read) {
        printf("%s  %smust precede IDAT\n",
               verbose? ":":fname, verbose? "":"tRNS ");
        set_err(1);
      }
      switch (ityp) {
        case 0:
          if (sz != 2) {
            printf("%s  incorrect %slength for %s image\n",
                   verbose? ":":fname, verbose? "":"tRNS ",png_type[ityp]);
            set_err(2);
          } else if (verbose && no_err(1)) {
            printf(": gray = %d\n", SH(buffer));
          }
          break;
        case 2:
          if (sz != 6) {
            printf("%s  incorrect %slength for %s image\n",
                   verbose? ":":fname, verbose? "":"tRNS ",png_type[ityp]);
            set_err(2);
          } else if (verbose && no_err(1)) {
            printf(": red = %d green = %d blue = %d\n", SH(buffer),
                   SH(buffer+2), SH(buffer+4));
          }
          break;
        case 3:
          if (sz > nplte) {
            printf("%s  incorrect %slength for %s image\n",
                   verbose? ":":fname, verbose? "":"tRNS ", png_type[ityp]);
            set_err(2);

          } else if ((verbose || (printpal && !quiet)) && no_err(1)) {
            if (printpal && !quiet)
              printf("  tRNS chunk");
            printf(": %ld transparency entr%s\n", sz, sz == 1? "y":"ies");
          }
          if (printpal && no_err(1)) {
            char *spc;

            if (sz < 10)
              spc = "  ";
            else if (sz < 100)
              spc = "   ";
            else
              spc = "    ";
            for (i = 0;  i < sz;  ++i)
              printf("%s%3d:  %3d = 0x%02x\n", spc, i, buffer[i], buffer[i]);
          }
          break;
        default:
          printf("%s  %snot allowed in %s image\n",
                 verbose? ":":fname, verbose? "":"tRNS ", png_type[ityp]);
          set_err(1);
          break;
      }
      have_trns = 1;
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | mkBF | 
     *------*/
    } else if (strcmp(chunkid, "mkBF") == 0) {
      if (verbose)
        printf("\n    "
          "Macromedia Fireworks private, ancillary, unsafe-to-copy chunk\n");
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | mkBS | 
     *------*/
    } else if (strcmp(chunkid, "mkBS") == 0) {
      if (verbose)
        printf("\n    "
          "Macromedia Fireworks private, ancillary, unsafe-to-copy chunk\n");
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | mkBT | 
     *------*/
    } else if (strcmp(chunkid, "mkBT") == 0) {
      if (verbose)
        printf("\n    "
          "Macromedia Fireworks private, ancillary, unsafe-to-copy chunk\n");
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | prVW | 
     *------*/
    } else if (strcmp(chunkid, "prVW") == 0) {
      if (verbose)
        printf("\n    Macromedia Fireworks preview chunk"
          " (private, ancillary, unsafe to copy)\n");
      last_is_idat = last_is_jdat = 0;

    /*================================================*
     * JNG chunks (with the exception of JHDR, above) *
     *================================================*/

    /*------* 
     | JDAT | 
     *------*/
    } else if (strcmp(chunkid, "JDAT") == 0) {
      if (png) {
        printf("%s  JDAT not defined in PNG\n", verbose? ":":fname);
        set_err(1);
      } else if (jdat_read && !(last_is_jdat || last_is_idat)) {
        /* GRR:  need to check for consecutive IDATs within MNG segments */
        if (mng) {  /* reset things (GRR SEMI-HACK:  check for segments!) */
          jdat_read = 0;
          if (verbose)
            printf("\n");
        } else {
          printf(
            "%s  JDAT chunks must be consecutive or interleaved with IDATs\n",
            verbose? ":":fname);
          set_err(2);
          if (!force)
            return;
        }
      } else if (verbose)
        printf("\n");
      jdat_read = 1;
      last_is_idat = 0;
      last_is_jdat = 1;   /* also true if last was JSEP (see below) */

    /*------* 
     | JSEP | 
     *------*/
    } else if (strcmp(chunkid, "JSEP") == 0) {
      if (png) {
        printf("%s  JSEP not defined in PNG\n", verbose? ":":fname);
        set_err(1);
      } else if (jng && bitdepth != 20) {
        printf("%s  JSEP allowed only if 8-bit and 12-bit JDATs present\n",
          verbose? ":":fname);
        set_err(1);
      } else if (jng && jsep_read) {
        printf("%s  multiple JSEP not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (jng && !(last_is_jdat || last_is_idat)) {
        printf("%s  JSEP must appear between JDAT or IDAT chunks\n",
          verbose? ":":fname);
        set_err(1);
      } else if (sz != 0) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"JSEP ");
        set_err(1);
      } else if (verbose) {
        printf("\n");
      }
      jsep_read = 1;
      last_is_idat = 0;
      last_is_jdat = 1;   /* effectively... (GRR HACK) */

    /*===============================================================*
     * MNG chunks (with the exception of MHDR and JNG chunks, above) *
     *===============================================================*/

    /*------* 
     | DHDR | 		DELTA-PNG
     *------*/
    } else if (strcmp(chunkid, "DHDR") == 0) {
      if (png || jng) {
        printf("%s  DHDR not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz != 4 && sz != 12 && sz != 20) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"DHDR ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        uch dtype = buffer[3];

        printf("\n    object ID = %u, image type = %s, delta type = %s\n",
          SH(buffer), buffer[2]? "PNG":"unspecified",
          (dtype < sizeof(delta_type)/sizeof(char *))?
          delta_type[dtype] : inv);
        if (sz > 4) {
          if (dtype == 5) {
            printf("%s  incorrect %slength for delta type %d\n",
              verbose? ":":fname, verbose? "":"DHDR ", dtype);
            set_err(1);
          } else {
            printf("    block width = %lu, block height = %lu\n", LG(buffer+4),
              LG(buffer+8));
            if (sz > 12) {
              if (dtype == 0 || dtype == 5) {
                printf("%s  incorrect %slength for delta type %d\n",
                  verbose? ":":fname, verbose? "":"DHDR ", dtype);
                set_err(1);
              } else
                printf("    x offset = %lu, y offset = %lu\n", LG(buffer+12),
                  LG(buffer+16));
            }
          }
        }
      }
      dhdr_read = 1;
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | FRAM | 
     *------*/
    } else if (strcmp(chunkid, "FRAM") == 0) {
      if (png || jng) {
        printf("%s  FRAM not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz == 0 && verbose) {
        printf(":  empty\n");
      } else if (verbose) {
        uch fmode = buffer[0];

        printf(": mode %d\n    %s\n", fmode,
          (fmode < sizeof(framing_mode)/sizeof(char *))?
          framing_mode[fmode] : inv);
        if (sz > 1) {
          uch *p = buffer+1;
          int bytes_left, found_null=0;

          if (*p) {
            printf("    frame name = ");
            do {
              if (*p)
                putchar(*p);	/* GRR EBCDIC WARNING */
              else {
                putchar('\n');
                ++p;
                break;
              }
            } while (++p < buffer + sz);
          } else {
            ++p;  /* skip over null */
            ++found_null;
          }
          bytes_left = sz - (p-buffer);
          if (bytes_left == 0 && found_null) {
            printf("    invalid trailing NULL byte\n");
            set_err(1);
          } else if (bytes_left < 4) {
            printf("    incorrect length\n");
            set_err(2);
          } else {
            uch cid = *p++;	/* change_interframe_delay */
            uch ctt = *p++;	/* change_timeout_and_termination */
            uch cscb = *p++;	/* change_subframe_clipping_boundaries */
            uch csil = *p++;	/* change_sync_id_list */

            if (cid > 2 || ctt > 8 || cscb > 2 || csil > 2) {
              printf("    invalid change flags\n");
              set_err(1);
            } else {
              bytes_left -= 4;
              printf("    %s\n", change_interframe_delay[cid]);
              /* GRR:  need real error-checking here: */
              if (cid && bytes_left >= 4) {
                ulg delay = LG(p);

                printf("      new delay = %lu tick%s\n", delay, (delay == 1L)?
                  "" : "s");
                p += 4;
                bytes_left -= 4;
              }
              printf("    %s\n", change_timeout_and_termination[ctt]);
              /* GRR:  need real error-checking here: */
              if (ctt && bytes_left >= 4) {
                ulg val = LG(p);

                if (val == 0x7fffffffL)
                  printf("      new timeout = infinite\n");
                else
                  printf("      new timeout = %lu tick%s\n", val, (val == 1L)?
                    "" : "s");
                p += 4;
                bytes_left -= 4;
              }
              printf("    %s\n", change_subframe_clipping_boundaries[cscb]);
              /* GRR:  need real error-checking here: */
              if (cscb && bytes_left >= 17) {
                printf("      new frame clipping boundaries (%s):\n", (*p++)?
                  "differences from previous values":"absolute pixel values");
                printf(
                  "        left = %ld, right = %ld, top = %ld, bottom = %ld\n",
                  LG(p), LG(p+4), LG(p+8), LG(p+12));
                p += 16;
                bytes_left -= 17;
              }
              printf("    %s\n", change_sync_id_list[csil]);
              if (csil) {
                if (bytes_left) {
                  while (bytes_left >= 4) {
                    printf("      %lu\n", LG(p));
                    p += 4;
                    bytes_left -= 4;
                  }
                } else
                  printf("      [empty list]\n");
              }
            }
          }
/*
          if (p < buffer + sz)
            printf("    (bytes left = %d)\n", sz - (p-buffer));
          else
            printf("    (no bytes left)\n");
 */
        }
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | SAVE | 
     *------*/
    } else if (strcmp(chunkid, "SAVE") == 0) {
      if (png || jng) {
        printf("%s  SAVE not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz > 0 && verbose) {
        uch offsize = buffer[0];

        if (offsize != 4 && offsize != 8) {
          printf("%s  incorrect %soffset size (%u bytes)\n",
            verbose? ":":fname, verbose? "":"SAVE ", (unsigned)offsize);
          set_err(1);
        } else if (sz > 1) {
          uch *p = buffer+1;
          int bytes_left = sz-1;

          printf("\n    offset size = %u bytes\n", (unsigned)offsize);
          while (bytes_left > 0) {
            uch type = *p;

            if ((type == 0 && bytes_left < 5+2*offsize) ||
                (type == 1 && bytes_left < 1+offsize)) {
              printf("%s  incorrect %slength\n",
                verbose? ":":fname, verbose? "":"SAVE ");
              set_err(1);
              break;
            }
            printf("    entry type = %s", (type <
              sizeof(entry_type)/sizeof(char *))? entry_type[type] : inv);
            ++p;
            if (type <= 1) {
              ulg first4 = LG(p);

              printf(", offset = ");
              if ((offsize == 4 && first4 == 0L) ||
                  (offsize == 8 && first4 == 0L && LG(p+4) == 0L))
                printf("unknown\n");
              else if (offsize == 4)
                printf("0x%04lx\n", first4);
              else
                printf("0x%04lx%04lx\n", first4, LG(p+4));  /* big-endian */
              p += offsize;
              if (type == 0) {
                printf("    nominal start time = 0x%04lx", LG(p));
                if (offsize == 8)
                  printf("%04lx", LG(p+4));
                p += offsize;
                printf(" , nominal frame number = %lu\n", LG(p));
                p += 4;
              }
            } else
              printf("\n");
            bytes_left = sz - (p-buffer);
            if (bytes_left) {
              do {
                if (*p)
                  putchar(*p);		/* GRR EBCDIC WARNING */
                else {
                  ++p;
                  break;
                }
              } while (++p < buffer + sz);
              bytes_left = sz - (p-buffer);
            }
          } /* end while (bytes_left > 0) */
        }
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | SEEK | 
     *------*/
    } else if (strcmp(chunkid, "SEEK") == 0) {
      if (png || jng) {
        printf("%s  SEEK not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz > 0 && verbose) {
        printf("\n    ");
        init_printbuffer(fname);
        printbuffer(buffer, sz, 0);
        finish_printbuffer(fname, chunkid);
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | nEED | 
     *------*/
    } else if (strcmp(chunkid, "nEED") == 0) {
      if (png || jng) {
        printf("%s  nEED not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz > 0 && verbose) {
        uch *p = buffer;
        uch *lastbreak = buffer;

        if (sz < 32)
          printf(":  ");
        else
          printf("\n    ");
        do {
          if (*p)
            putchar(*p);	/* GRR EBCDIC WARNING */
          else if (p - lastbreak > 40)
            printf("\n    ");
          else {
            putchar(';');
            putchar(' ');
          }
        } while (++p < buffer + sz);
        printf("\n");
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | DEFI | 
     *------*/
    } else if (strcmp(chunkid, "DEFI") == 0) {
      if (png || jng) {
        printf("%s  DEFI not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz != 2 && sz != 3 && sz != 4 && sz != 12 && sz != 28) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"DEFI ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        char *noshow = do_not_show[0];
        uch concrete = 0;
        long x = 0L;
        long y = 0L;

        if (sz > 2) {
          if (buffer[2] == 1)
            noshow = do_not_show[1];
          else if (buffer[2] > 1)
            noshow = inv;
        }
        if (sz > 3)
          concrete = buffer[3];
        if (sz > 4) {
          x = LG(buffer+4);
          y = LG(buffer+8);
        }
        printf("\n    object ID = %u, %s, %s, x = %ld, y = %ld\n", SH(buffer),
          noshow, concrete? "concrete":"abstract", x, y);
        if (sz > 12) {
          printf(
            "    clipping:  left = %ld, right = %ld, top = %ld, bottom = %ld\n",
            LG(buffer+12), LG(buffer+16), LG(buffer+20), LG(buffer+24));
        }
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | BACK | 
     *------*/
    } else if (strcmp(chunkid, "BACK") == 0) {
      if (png || jng) {
        printf("%s  BACK not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz != 6 && sz != 7) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"BACK ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        printf("\n    red = %u, green = %u, blue = %u (%s)\n",
          SH(buffer), SH(buffer+2), SH(buffer+4), (sz > 6 && buffer[6])?
          "mandatory":"advisory");
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | MOVE | 
     *------*/
    } else if (strcmp(chunkid, "MOVE") == 0) {
      if (png || jng) {
        printf("%s  MOVE not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz != 13) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"MOVE ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        printf("\n    first object ID = %u, last object ID = %u\n",
          SH(buffer), SH(buffer+2));
        if (buffer[4])
          printf(
            "    relative change in position:  delta-x = %ld, delta-y = %ld\n",
            LG(buffer+5), LG(buffer+9));
        else
          printf("    new position:  x = %ld, y = %ld\n",
            LG(buffer+5), LG(buffer+9));
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | CLON | 
     *------*/
    } else if (strcmp(chunkid, "CLON") == 0) {
      if (png || jng) {
        printf("%s  CLON not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz != 4 && sz != 5 && sz != 6 && sz != 7 && sz != 16) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"CLON ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        uch ct = 0;	/* full clone */
        uch dns = 2;	/* same as parent's */
        uch cf = 0;	/* same as parent's */
        uch ldt = 1;	/* delta from parent */
        long x = 0L;
        long y = 0L;

        if (sz > 4)
          ct = buffer[4];
        if (sz > 5)
          dns = buffer[5];
        if (sz > 6)
          cf = buffer[6];
        if (sz > 7) {
          ldt = buffer[7];
          x = LG(buffer+8);
          y = LG(buffer+12);
        }
        printf("\n    parent object ID = %u, clone object ID = %u\n",
          SH(buffer), SH(buffer+2));
        printf("    clone type = %s, %s, %s\n",
          (ct < sizeof(clone_type)/sizeof(char *))? clone_type[ct] : inv,
          (dns < sizeof(do_not_show)/sizeof(char *))? do_not_show[dns] : inv,
          cf? "same concreteness as parent":"abstract");
        if (ldt)
          printf("    difference from parent's position:  delta-x = %ld,"
            " delta-y = %ld\n", x, y);
        else
          printf("    absolute position:  x = %ld, y = %ld\n", x, y);
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | SHOW | 
     *------*/
    } else if (strcmp(chunkid, "SHOW") == 0) {
      if (png || jng) {
        printf("%s  SHOW not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz != 0 && sz != 2 && sz != 4 && sz != 5) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"SHOW ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        ush first = 0;
        ush last = 65535;
        uch smode = 2;

        if (sz > 0) {
          first = last = SH(buffer);
          smode = 0;
        }
        if (sz > 2)
          last = SH(buffer+2);
        if (sz > 4)
          smode = buffer[4];
        printf("\n    first object = %u, last object = %u\n", first, last);
        printf("    %s\n",
          (smode < sizeof(show_mode)/sizeof(char *))? show_mode[smode] : inv);
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | CLIP | 
     *------*/
    } else if (strcmp(chunkid, "CLIP") == 0) {
      if (png || jng) {
        printf("%s  CLIP not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz != 21) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"CLIP ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        printf(
          "\n    first object = %u, last object = %u; %s clip boundaries:\n",
          SH(buffer), SH(buffer+2), buffer[4]? "relative change in":"absolute");
        printf("      left = %ld, right = %ld, top = %ld, bottom = %ld\n",
          LG(buffer+5), LG(buffer+9), LG(buffer+13), LG(buffer+17));
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | LOOP | 
     *------*/
    } else if (strcmp(chunkid, "LOOP") == 0) {
      if (png || jng) {
        printf("%s  LOOP not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz < 5 || (sz > 6 && ((sz-6) % 4) != 0)) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"LOOP ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        printf(":  nest level = %u\n    count = %lu, termination = %s\n",
          (unsigned)(buffer[0]), LG(buffer+1), sz == 5?
          termination_condition[0] : termination_condition[buffer[5] & 0x3]);
          /* GRR:  not checking for valid buffer[1] values */
        if (sz > 6) {
          printf("    iteration min = %lu", LG(buffer+6));
          if (sz > 10) {
            printf(", max = %lu", LG(buffer+10));
            if (sz > 14) {
              long i, count = (sz-14) >> 2;

              printf(", signal number%s = %lu", (count > 1)? "s" : "",
                LG(buffer+14));
              for (i = 1;  i < count;  ++i)
                printf(", %lu", LG(buffer+14+(i<<2)));
            }
          }
          printf("\n");
        }
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | ENDL | 
     *------*/
    } else if (strcmp(chunkid, "ENDL") == 0) {
      if (png || jng) {
        printf("%s  ENDL not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz != 1) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"ENDL ");
        set_err(2);
      }
      if (verbose && no_err(1))
        printf(":  nest level = %u\n", (unsigned)(buffer[0]));
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | PROM | 
     *------*/
    } else if (strcmp(chunkid, "PROM") == 0) {
      if (png || jng) {
        printf("%s  PROM not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz != 3) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"PROM ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        char *ctype;

        switch (buffer[0]) {
          case 2:
            ctype = "gray+alpha";
            break;
          case 4:
            ctype = "RGB";
            break;
          case 6:
            ctype = "RGBA";
            break;
          default:
            ctype = "INVALID";
            set_err(1);
            break;
        }
        printf("\n    new color type = %s, new bit depth = %u\n",
          ctype, (unsigned)(buffer[1]));
          /* GRR:  not checking for valid buffer[1] values */
        printf("    fill method (if bit depth increased) = %s\n",
          buffer[2]? "zero fill" : "left bit replication");
          /* GRR:  not checking for valid buffer[2] values */
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | fPRI | 
     *------*/
    } else if (strcmp(chunkid, "fPRI") == 0) {
      if (png || jng) {
        printf("%s  fPRI not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz != 2) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"fPRI ");
        set_err(2);
      }
      if (verbose && no_err(1))
        printf(":  %spriority = %u\n", buffer[0]? "delta " : "",
          (unsigned)(buffer[1]));
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | eXPI | 
     *------*/
    } else if (strcmp(chunkid, "eXPI") == 0) {
      if (png || jng) {
        printf("%s  eXPI not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz <= 2) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"eXPI ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        printf("\n    snapshot ID = %u, snapshot name = %.*s\n", SH(buffer),
          sz-2, buffer+2);	/* GRR EBCDIC WARNING */
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | BASI | 
     *------*/
    } else if (strcmp(chunkid, "BASI") == 0) {
      if (png || jng) {
        printf("%s  BASI not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz != 13 && sz != 19 && sz != 22) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"BASI ");
        set_err(2);
      }
      if (no_err(1)) {
        w = LG(buffer);
        h = LG(buffer+4);
        if (w == 0 || h == 0) {
          printf("%s  invalid %simage dimensions (%ldx%ld)\n",
            verbose? ":":fname, verbose? "":"BASI ", w, h);
          set_err(1);
        }
        bitdepth = (uch)buffer[8];
        ityp = (uch)buffer[9];
        if (ityp > sizeof(png_type)/sizeof(char*)) {
          ityp = 1; /* avoid out of range array index */
        }
        switch (bitdepth) {
          case 1:
          case 2:
          case 4:
            if (ityp == 2 || ityp == 4 || ityp == 6) {  /* RGB or GA or RGBA */
              printf("%s  invalid %sbit depth (%d) for %s image\n",
                verbose? ":":fname, verbose? "":"BASI ", bitdepth, png_type[ityp]);
              set_err(1);
            }
            break;
          case 8:
            break;
          case 16:
            if (ityp == 3) { /* palette */
              printf("%s  invalid %sbit depth (%d) for %s image\n",
                verbose? ":":fname, verbose? "":"BASI ", bitdepth, png_type[ityp]);
              set_err(1);
            }
            break;
          default:
            printf("%s  invalid %sbit depth (%d)\n",
              verbose? ":":fname, verbose? "":"BASI ", bitdepth);
            set_err(1);
            break;
        }
        lace = (uch)buffer[12];
        switch (ityp) {
          case 2:
            bitdepth *= 3;   /* RGB */
            break;
          case 4:
            bitdepth *= 2;   /* gray+alpha */
            break;
          case 6:
            bitdepth *= 4;   /* RGBA */
            break;
        }
        if (verbose && no_err(1)) {
          printf("\n    %ld x %ld image, %d-bit %s, %sinterlaced\n", w, h,
            bitdepth, (ityp > 6)? png_type[1]:png_type[ityp], lace? "":"non-");
        }
        if (sz > 13) {
          ush red, green, blue;
          long alpha = -1;
          int viewable = -1;

          red = SH(buffer+13);
          green = SH(buffer+15);
          blue = SH(buffer+17);
          if (sz > 19) {
            alpha = (long)SH(buffer+19);
            if (sz > 21)
                viewable = buffer[21];
          }
          if (verbose && no_err(1)) {
            if (ityp == 0)
              printf("    gray = %u", red);
            else
              printf("    red = %u green = %u blue = %u", red, green, blue);
            if (alpha >= 0) {
              printf(", alpha = %ld", alpha);
              if (viewable >= 0)
                printf(", %sviewable", viewable? "" : "not ");
            }
            printf("\n");
          }
        }
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | IPNG |    (empty stand-in for IHDR)
     *------*/
    } else if (strcmp(chunkid, "IPNG") == 0) {
      if (png || jng) {
        printf("%s  IPNG not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz != 0) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"IPNG ");
        set_err(1);
      } else if (verbose) {
        printf("\n");
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | PPLT | 
     *------*/
    } else if (strcmp(chunkid, "PPLT") == 0) {
      if (png || jng) {
        printf("%s  PPLT not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (sz < 4) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"PPLT ");
        set_err(1);
      } else {
        char *plus;
        uch dtype = buffer[0];
        uch first_idx = buffer[1];
        uch last_idx = buffer[2];
        uch *buf = buffer+3;
        int bytes_left = sz-3;
        int samples, npplt = 0, nblks = 0;

        if (!verbose && printpal && !quiet)
          printf("  PPLT chunk");
        if (verbose)
          printf(": %s\n", (dtype < sizeof(pplt_delta_type)/sizeof(char *))?
            pplt_delta_type[dtype] : inv);
        plus = (dtype & 1)? "+" : "";
        if (dtype < 2)
          samples = 3;
        else if (dtype < 4)
          samples = 1;
        else
          samples = 4;
        while (bytes_left > 0) {
          bytes_left -= samples*(last_idx - first_idx + 1);
          if (bytes_left < 0)
            break;
          ++nblks;
          for (i = first_idx;  i <= last_idx;  ++i, buf += samples) {
            ++npplt;
            if (printpal) {
              if (samples == 4)
                printf("    %3d:  %s(%3d,%3d,%3d,%3d) = "
                  "%s(0x%02x,0x%02x,0x%02x,0x%02x)\n", i,
                  plus, buf[0], buf[1], buf[2], buf[3],
                  plus, buf[0], buf[1], buf[2], buf[3]);
              else if (samples == 3)
                printf("    %3d:  %s(%3d,%3d,%3d) = %s(0x%02x,0x%02x,0x%02x)\n",
                  i, plus, buf[0], buf[1], buf[2],
                  plus, buf[0], buf[1], buf[2]);
              else
                printf("    %3d:  %s(%3d) = %s(0x%02x)\n", i,
                  plus, *buf, plus, *buf);
            }
          }
          if (bytes_left > 2) {
            first_idx = buf[0];
            last_idx = buf[1];
            buf += 2;
            bytes_left -= 2;
          } else if (bytes_left)
            break;
        }
        if (bytes_left) {
          printf("%s  incorrect %slength (too %s bytes)\n",
            verbose? ":" : fname, verbose? "" : "PPLT ",
            (bytes_left < 0)? "few" : "many");
          set_err(1);
        }
        if (verbose && no_err(1))
          printf("    %d %s palette entr%s in %d block%s\n",
            npplt, (dtype & 1)? "delta" : "replacement", npplt== 1? "y":"ies",
            nblks, nblks== 1? "":"s");
      }
      last_is_idat = last_is_jdat = 0;

    /*------* 
     | MEND | 
     *------*/
    } else if (strcmp(chunkid, "MEND") == 0) {
      if (png || jng) {
        printf("%s  MEND not defined in %cNG\n", verbose? ":":fname,
          png? 'P':'J');
        set_err(1);
      } else if (mend_read) {
        printf("%s  multiple MEND not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (sz != 0) {
        printf("%s  incorrect %slength\n",
               verbose? ":":fname, verbose? "":"MEND ");
        set_err(1);
      } else if (verbose) {
        printf("\n");
      }
      mend_read = 1;
      last_is_idat = last_is_jdat = 0;

    /*===============*
     * unknown chunk *
     *===============*/

    } else {
      /* A critical safe-to-copy chunk is an error */
      if (!(chunkid[0] & 0x20) && chunkid[3] & 0x20) {
        printf("%s  illegal critical, safe-to-copy chunk%s%s\n",
               verbose? ":":fname, verbose? "":" ", verbose? "":chunkid);
        set_err(1);
      }
      else if (verbose) {
        printf("\n    unknown %s, %s, %s%ssafe-to-copy chunk\n",
               chunkid[1] & 0x20 ? "private":"public",
               chunkid[0] & 0x20 ? "ancillary":"critical",
               chunkid[2] & 0x20 ? "reserved-bit-set, ":"",
               chunkid[3] & 0x20 ? "":"un");
      }
      last_is_idat = last_is_jdat = 0;
    }

    /*=======================================================================*/

    if (no_err(1)) {
      if (fpOut != NULL) {
        putlong(fpOut, sz);
        (void)fwrite(chunkid, 1, 4, fpOut);
        (void)fwrite(buffer, 1, toread, fpOut);
      }

      while (sz > toread) {
        int data_read;
        sz -= toread;
        toread = (sz > BS)? BS:sz;

        data_read = fread(buffer, 1, toread, fp);
        if (fpOut != NULL) (void)fwrite(buffer, 1, data_read, fpOut);

        if (data_read != toread) {
          printf("%s  EOF while reading %s%sdata\n",
                 verbose? ":":fname, verbose? "":chunkid, verbose? "":" ");
          set_err(3);
          return;
        }

        crc = update_crc(crc, (uch *)buffer, toread);
      }

      filecrc = getlong(fp, fname, "crc value");

      if (is_err(2))
        return;

      if (filecrc != CRCCOMPL(crc)) {
        printf("%s  CRC error in chunk %s (actual %08lx, should be %08lx)\n",
               verbose? "":fname, chunkid, CRCCOMPL(crc), filecrc);
        set_err(1);
      }

      if (no_err(1) && fpOut != NULL)
        putlong(fpOut, CRCCOMPL(crc));
    }

    if (error > 0 && !force)
      return;
  }

  /*----------------------- END OF IMMENSE WHILE-LOOP -----------------------*/

  if ((png || jng) && !iend_read || mng && !mend_read) {
    printf("%s  file doesn't end with a%sEND chunk\n", verbose? "":fname,
      mng? " M":"n I");
    set_err(1);
    return;
  }

  /* GRR 19970621: print compression ratio based on file size vs. byte-packed
   *   raw data size.  Arguably it might be fairer to compare against the size
   *   of the unadorned, compressed data, but since PNG is a package deal...
   * GRR 19990619: disabled for MNG, at least until we figure out a reasonable
   *   way to calculate the ratio; also switched to MNG-relevant stats. */

  if (error == 0) {
    if (mng) {
      if (verbose) {  /* already printed MHDR/IHDR/JHDR info */
        printf("No errors detected in %s (%ld chunks).\n", fname, num_chunks);
      } else if (!quiet) {
/*
        printf("No errors detected in %s (%ldx%ld", fname,
          mng_width, mng_height);
 */
        printf("OK: %s (%ldx%ld", fname,
          mng_width, mng_height);
        if (vlc == 1)
          printf(", VLC");
        else if (lc == 1)
          printf(", LC");
        if (layers && layers < 0x7ffffffL)
          printf(", %lu layer%s", layers, (layers == 1L)? "" : "s");
        if (frames && frames < 0x7ffffffL)
          printf(", %lu frame%s", frames, (frames == 1L)? "" : "s");
        printf(").\n");
      }

    } else if (jng) {
      char *sgn = "";
      int cfactor;
      ulg ucsize;

      if (!did_stat) {
        stat(fname, &statbuf);   /* already know file exists; don't check rc */
      }

      /* uncompressed size (bytes), compressed size => returns 10*ratio (%) */
      if (bitdepth == 20)
        ucsize = h*(w + ((w*12+7)>>3));
      else
        ucsize = h*((w*bitdepth+7)>>3);
      if (alphadepth > 0)
        ucsize += h*((w*alphadepth+7)>>3);
      if ((cfactor = ratio(ucsize, statbuf.st_size)) < 0)
      {
        sgn = "-";
        cfactor = -cfactor;
      }

      if (verbose) {  /* already printed JHDR info */
        printf("No errors detected in %s (%s%d.%d%% compression).\n", fname,
          sgn, cfactor/10, cfactor%10);
      } else if (!quiet) {
        if (jtyp < 2)
          printf("OK: %s (%ldx%ld, %d-bit %s%s%s, %s%d.%d%%).\n",
            fname, w, h, jbitd, and, jng_type[jtyp], lace? ", progressive":"",
            sgn, cfactor/10, cfactor%10);
        else
          printf("OK: %s (%ldx%ld, %d-bit %s%s + %d-bit alpha%s, %s%d.%d%%).\n",
            fname, w, h, jbitd, and, jng_type[jtyp-2], alphadepth,
            lace? ", progressive":"", sgn, cfactor/10, cfactor%10);
      }

    } else {
      char *sgn = "";
      int cfactor;

      if (!did_stat) {
        stat(fname, &statbuf);   /* already know file exists */
      }

      /* uncompressed size (bytes), compressed size => returns 10*ratio (%) */
      if ((cfactor = ratio((ulg)(h*((w*bitdepth+7)>>3)), statbuf.st_size)) < 0)
      {
        sgn = "-";
        cfactor = -cfactor;
      }

      if (verbose) {  /* already printed IHDR/JHDR info */
        printf("No errors detected in %s (%s%d.%d%% compression).\n", fname,
          sgn, cfactor/10, cfactor%10);
      } else if (!quiet) {
/*
        printf("No errors detected in %s\n  (%ldx%ld, %d-bit %s%s, "
          "%sinterlaced, %s%d.%d%%).\n",
          fname, w, h, bitdepth, (ityp > 6)? png_type[1] : png_type[ityp],
          (ityp == 3 && have_trns)? "+trns" : "", lace? "" : "non-",
          sgn, cfactor/10, cfactor%10);
 */
        printf("OK: %s (%ldx%ld, %d-bit %s%s, %sinterlaced, %s%d.%d%%).\n",
          fname, w, h, bitdepth, (ityp > 6)? png_type[1] : png_type[ityp],
          (ityp == 3 && have_trns)? "+trns" : "", lace? "" : "non-",
          sgn, cfactor/10, cfactor%10);
      }
    }

  }
}

void pnginfile(FILE *fp, char *fname, int ipng, int extracting)
{
  char name[1024], *szdot;
  FILE *fpOut = NULL;

#if 1
  strncpy(name, fname, 1024-20);
  name[1024-20] = 0;
  szdot = strrchr(name, '.');
  if (szdot == NULL)
    szdot = name + strlen(name);
  sprintf(szdot, "-%d", ipng);
#else
  /* Use this if filename length is restricted. */
  sprintf(name, "PNG%d", ipng);
  szdot = name;
#endif

  if (extracting) {
    szdot += strlen(szdot);
    strcpy(szdot, ".png");
    fpOut = fopen(name, "wb");
    if (fpOut == NULL) {
      perror(name);
      fprintf(stderr, "%s: could not write output (ignored)\n", name);
    } else if (verbose) {
      printf("%s: contains %s PNG %d\n", name, fname, ipng);
    }
    (void)fwrite(good_PNG_magic, 8, 1, fpOut);
    *szdot = 0;
  }

  pngcheck(fp, name, 1, fpOut);

  if (fpOut != NULL) {
    if (ferror(fpOut) != 0 || fclose(fpOut) != 0) {
      perror(name); /* Will only show most recent error */
      fprintf(stderr, "%s: error on output (ignored)\n", name);
    }
  }
}

void pngsearch(FILE *fp, char *fname, int extracting)
{
  /* Go through the file looking for a PNG magic number, if one is
     found check the data to see if it is a PNG and validate the
     contents.  Useful when something puts a PNG in something else. */
  int ch;
  int ipng = 0;
 
  if (verbose)
    printf("Scanning: %s\n", fname);
  else if (extracting)
    printf("Extracting PNGs from %s\n", fname);

  /* This works because the leading 137 code is not repeated in the
     magic, so ANSI C says we will break out of the comparison as
     soon as the partial match fails, and we can start a new test.
   */
  do {
    ch = getc(fp);
    while (ch == good_PNG_magic[0]) {
      if ((ch = getc(fp)) == good_PNG_magic[1] &&
          (ch = getc(fp)) == good_PNG_magic[2] &&
          (ch = getc(fp)) == good_PNG_magic[3] &&
          (ch = getc(fp)) == good_PNG_magic[4] &&
          (ch = getc(fp)) == good_PNG_magic[5] &&
          (ch = getc(fp)) == good_PNG_magic[6] &&
          (ch = getc(fp)) == good_PNG_magic[7]) {
        /* Just after a PNG header. */
        pnginfile(fp, fname, ++ipng, extracting);
      }
    }
  } while (ch != EOF);
}

int main(int argc, char *argv[])
{
  FILE *fp;
  int i = 1;

#ifdef __EMX__
  _wildcard(&argc, &argv);   /* Unix-like globbing for OS/2 and DOS */
#endif

  while(argc > 1 && argv[1][0] == '-') {
    switch(argv[1][i]) {
      case '\0':
        argc--;
        argv++;
        i = 1;
        break;
      case '7':
        printtext=1;
        sevenbit=1;
        i++;
        break;
      case 'f':
        force=1;
        i++;
        break;
      case 'h':
        goto usage;
        break;
      case 'p':
        printpal=1;
        i++;
        break;
      case 'q':
        verbose=0;
        quiet=1;
        i++;
        break;
      case 's':
        search=1;
        i++;
        break;
      case 't':
        printtext=1;
        i++;
        break;
      case 'v':
        ++verbose;  /* verbose == 2 means decode IDATs and print filter info */
        quiet=0;
        i++;
        break;
      case 'x':
        search=extract=1;
        i++;
        break;
      default:
        fprintf(stderr, "unknown option %c\n", argv[1][i]);
        set_err(1);
        goto usage;
    }
  }

  if (argc == 1) {
    if (isatty(0)) { /* if stdin not redirected, give the user help */
usage:
      fprintf(stderr, "PNGcheck, version %s\n", VERSION);
      fprintf(stderr, "   by Alexander Lehmann, Andreas Dilger and Greg Roelofs.\n");
#ifdef USE_ZLIB
      fprintf(stderr, "   Compiled with zlib %s; using zlib %s.\n",
        ZLIB_VERSION, zlib_version);
#endif
      fprintf(stderr, "\nTest PNG image files for corruption, and print size/type/compression info.\n\n");
      fprintf(stderr, "Usage:  pngcheck [-vqt7f] file.png [file.png [...]]\n");
      fprintf(stderr, "   or:  pngcheck [-vqt7f] file.mng [file.mng [...]]\n");
      fprintf(stderr, "   or:  ... | pngcheck [-sx][vqt7f]\n");
      fprintf(stderr, "   or:  pngcheck -{sx}[vqt7f] file-containing-PNGs...\n\n");
      fprintf(stderr, "Options:\n");
      fprintf(stderr, "   -v  test verbosely (print most chunk data)\n");
#ifdef USE_ZLIB
      fprintf(stderr, "   -vv test very verbosely (decode & print line filters)\n");
#endif
      fprintf(stderr, "   -q  test quietly (only output errors)\n");
      fprintf(stderr, "   -t  print contents of tEXt chunks (can be used with -q)\n");
      fprintf(stderr, "   -7  print contents of tEXt chunks, escape chars >=128 (for 7-bit terminals)\n");
      fprintf(stderr, "   -p  print contents of PLTE, tRNS, hIST, sPLT and PPLT (can be used with -q)\n");
      fprintf(stderr, "   -f  force continuation even after major errors\n");
      fprintf(stderr, "   -s  search for PNGs within another file\n");
      fprintf(stderr, "   -x  search for PNGs and extract them when found\n");
      fprintf(stderr, "\nNote:  MNG support is incomplete.  Based on MNG Draft 64.\n");
    } else {
      if (search)
        pngsearch(stdin, "stdin", extract);
      else
        pngcheck(stdin, "stdin", 0, NULL);
    }
  } else {
#ifdef USE_ZLIB
    if (verbose > 1) {
      /* make sure we're using the zlib version we were compiled to use */
      if (zlib_version[0] != ZLIB_VERSION[0]) {
        fflush(stdout);
        fprintf(stderr, "zlib error:  incompatible version (expected %s,"
          " using %s):  ignoring -vv\n\n", ZLIB_VERSION, zlib_version);
        fflush(stderr);
        verbose = 1;
      } else if (strcmp(zlib_version, ZLIB_VERSION) != 0) {
        fprintf(stderr, "zlib warning:  different version (expected %s,"
          " using %s)\n\n", ZLIB_VERSION, zlib_version);
        fflush(stderr);
      }
    }
#endif /* USE_ZLIB */
    for(i = 1; i < argc; i++) {
      /* This is somewhat ugly.  It sets the file pointer to stdin if the
       * filename is "-", otherwise it tries to open the given filename.
       */
      if ((fp = strcmp(argv[1],"-") == 0 ? stdin:fopen(argv[i],"rb")) == NULL) {
        perror(argv[i]);
        error = 2;
      } else {
        if (search)
          pngsearch(fp, fp == stdin? "stdin":argv[i], extract);
        else
          pngcheck(fp, fp == stdin? "stdin":argv[i], 0, NULL);
        fclose(fp);
      }
    }
  }

  /* This only returns the error on the last file.  Works OK if you are only
   * checking the status of a single file. */
  return error;
}

/*  PNG_subs
 *
 *  Utility routines for PNG encoders and decoders
 *  by Glenn Randers-Pehrson
 *
 */

/* check_magic()
 *
 * Check the magic numbers in 8-byte buffer at the beginning of
 * a (possible) PNG or MNG or JNG file.
 *
 * by Alexander Lehmann, Glenn Randers-Pehrson and Greg Roelofs
 *
 * This is free software; you can redistribute it and/or modify it
 * without any restrictions.
 *
 */

int check_magic(uch *magic, char *fname, int which)
{
  int i;
  uch *good_magic = (which == 0)? good_PNG_magic :
                    ((which == 1)? good_MNG_magic : good_JNG_magic);

  for(i = 1; i < 3; i++)
  {
    if (magic[i] != good_magic[i]) {
      return 2;
    }
  }

  if (magic[0] != good_magic[0] ||
      magic[4] != good_magic[4] || magic[5] != good_magic[5] ||
      magic[6] != good_magic[6] || magic[7] != good_magic[7]) {

    if (!verbose) {
      printf("%s:  File is CORRUPTED by text conversion.\n", fname);
      return 1;
    }

    printf("  File is CORRUPTED.  It seems to have suffered ");

    /* This coding derived from Alexander Lehmann's checkpng code   */
    if (strncmp((char *)&magic[4], "\012\032", 2) == 0)
      printf("DOS->Unix");
    else if (strncmp((char *)&magic[4], "\015\032", 2) == 0)
      printf("DOS->Mac");
    else if (strncmp((char *)&magic[4], "\015\015\032", 3) == 0)
      printf("Unix->Mac");
    else if (strncmp((char *)&magic[4], "\012\012\032", 3) == 0)
      printf("Mac->Unix");
    else if (strncmp((char *)&magic[4], "\012\012", 2) == 0)
      printf("DOS->Unix");
    else if (strncmp((char *)&magic[4], "\015\015\012\032", 4) == 0)
      printf("Unix->DOS");
    else if (strncmp((char *)&magic[4], "\015\012\032\015", 4) == 0)
      printf("Unix->DOS");
    else if (strncmp((char *)&magic[4], "\015\012\012", 3) == 0)
      printf("DOS EOF");
    else if (strncmp((char *)&magic[4], "\015\012\032\012", 4) != 0)
      printf("EOL");
    else
      printf("an unknown");

    printf(" conversion.\n");

    if (magic[0] == 9)
      printf("  It was probably transmitted through a 7-bit channel.\n");
    else if (magic[0] != good_magic[0])
      printf("  It was probably transmitted in text mode.\n");

    return 1;
  }

  return 0;
}



/* GRR:  not EBCDIC-safe! */
int check_chunk_name(char *chunk_name, char *fname)
{
  if (!isalpha(chunk_name[0]) || !isalpha(chunk_name[1]) ||
      !isalpha(chunk_name[2]) || !isalpha(chunk_name[3]))
  {
    printf(
      "%s%s  Chunk name %02x %02x %02x %02x doesn't conform to naming rules.\n",
      verbose? "":fname, verbose? "":":",
      chunk_name[0], chunk_name[1], chunk_name[2], chunk_name[3]);
    set_err(2);
    return 1;
  }

  return 0;
}
