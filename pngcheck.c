/*
 * Authenticate the structure of a PNG file and dump info about it if desired.
 *
 *        NOTE:  this program is currently NOT EBCDIC-compatible!
 *               (as of June 1998)
 *
 * This program checks the PNG identifier with conversion checks,
 * the file structure and the chunk CRCs. In addition, relations and contents
 * of most defined chunks are checked, except for decompression of the IDAT
 * chunks.
 *
 * Started by Alexander Lehmann <alex@hal.rhein-main.de> and subsequently
 * extended by people as listed below.
 *
 *  AL      -> Alexander Lehmann
 *  glennrp -> Glenn Randers-Pehrson
 *  GRR     -> Greg Roelofs
 *  AED     -> Andreas Dilger
 *  JB      -> John Bowler
 *
 * 95.02.23 AL: fixed wrong magic numbers
 *
 * 95.03.13 AL: crc code from png spec, compiles on memory impaired PCs now,
 *          check for IHDR/IEND chunks
 *
 * 95.03.25 glennrp rewrote magic number checking and moved it to
 *            PNG_check_magic(buffer)
 *
 * 95.03.27 AL: fixed CRC code for 64 bit, -t switch, unsigned char vs. char
 *          pointer changes
 *
 * 96.06.01 AL: check for data after IEND chunk
 *
 * 95.06.01 GRR: reformatted; print tEXt and zTXt keywords; added usage
 *
 * 95.07.31 AL: check for control chars, check for MacBinary header, new
 *          force option
 *
 * 95.08.27 AL: merged Greg's 1.61 changes: print IHDR and tIME contents,
 *          call emx wildcard function
 *
 * 95.11.21 AED: re-ordered internal chunk checking in pngcheck().
 *          Now decodes most of the known chunk types to check for invalid
 *          contents (except IDAT and zTXt).  Information is printed
 *          about the contents of known chunks, and unknown chunks have
 *          their chunk name flags decoded.  Also checks chunk ordering.
 *
 * 95.11.26 AED: minor bug fixes and nicening of the output.  Checks for
 *          valid cHRM contents per Chris Lilley's recommendation.
 *
 * 95.12.04 AED: minor bug in cHRM error output fixed
 *
 * 96.01.05 AED: added -q flaq to only output a message if an error occurrs
 *
 * 96.01.19 AED: added ability to parse multiple options with a single '-'
 *               changed tIME output to be in RFC 1123 format
 *
 * 96.05.17 GRR: fixed obvious(?) fprintf error; fixed multiple-tIME error msg
 *
 * 96.05.21 AL: fixed two tRNS errors reported by someone from W3C (whose name
 *              I currently don't remember) (complained about missing palette
 *              in greyscale images, missing breaks in case statement)
 *              avoid reference to undefined array entry with out of range ityp
 *
 * 96.06.05 AED: removed extra linefeed from cHRM output when not verbose
 *               added test to see if sBIT contents are valid
 *               added test for zero width or height in IHDR
 *               added test for insufficient IDAT data (minimum 10 bytes)
 *
 * 96.06.05 AED: added -p flag to dump the palette contents
 *
 * 96.12.31 JB: add decoding of the zlib header from the first IDAT chunk (16-
 *              bit header code in first two bytes, see print_zlibheader).
 *
 * 97.01.02 GRR: more sensible zlib-header output (version "1.97grr"); nuked
 *               some tabs; fixed blank lines between files in verbose output
 *
 * 97.01.06 AED: initialize the command-line flags
 *               add macros to ensure the error level doesn't go down
 *               return error level to calling program
 *               consolidate PNG magic on one place
 *               add "-" as input file for stdin
 *               check for valid tEXt/zTXt keywords per PNG Spec 1.0
 *               slight modification to output of tEXt/zTXt keywords/contents
 *               change 'extract' to only output valid chunks (unless forced)
 *                 this may allow one to fix minor errors in a PNG file
 *
 * 97.01.07 GRR: added USE_ZLIB compile option to print line filters (with -vv)
 * 97.01.10 GRR: fixed line-filters code for large-IDAT case
 * 97.06.21 GRR: added compression-ratio info
 * 98.06.09 TGL: fixed pHYs buglet
 * 98.06.09 GRR: re-integrated minimal MNG support from 97.01.21 branch
 * 98.06.10 GRR: extended MNG (MHDR info, DHDR, nEED, DEFI, FRAM, MEND)
 * 98.06.11 GRR: extended MNG (more FRAM info; LOOP, ENDL)
 * 98.06.12 GRR: extended MNG (FRAM, BACK, MOVE, CLON, SHOW, CLIP, fPRI, eXPI)
 * 98.06.16 GRR: extended MNG (PROM, SAVE, SEEK)
 * 98.07.02 GRR: fixed line-filters bug reported by Theodore Goodman (97.10.19);
 *               updated SAVE for MNG Draft 43
 * 98.07.11 GRR: added sPLT; extended printpal (-p) to support tRNS, hIST, sPLT
 * 98.10.21 GRR: added Win32 fix and compilation info; fixed mng=0, DEFI and
 *               printpal bugs
 */

/*
 * GRR NOTE:  current MNG support is informational; error-checking is MINIMAL!
 */

/*
 * GRR to do:
 *   [update for MNG Draft 51]
 *   [MNG chunks:  BASI, PAST, DISC, tERm, IPNG, DROP, DBYK, ORDR]
 *   [PNJ chunks:  JHDR, etc.]
 *   [PNG chunks:  pCAL, sRGB, fRAc, gIFg, gIFt, gIFx]
 *   [fix or disable compression ratio and non-"-v" summary info for MNGs]
 *   [fix basiflag_d43.mng "not enough IDAT data" bug (i.e., allow no IDATs)]
 *   [split out each chunk's code into handle_XXXX() function]
 *   [DOS/Win32 wildcard support]
 *   [EBCDIC support (minimal?)]
 */

/*
 * GRR unreported spec problems:
 *
 * MNG Draft 43: [reported but not checked]
 *  - PROM:  cannot promote bit depth without promoting color type?
 *           (else "cases" incomplete)
 *  - SEEK:  why is null byte allowed?
 *  - SHOW:  show_mode=7: missing end quote: "do_not_show=1.
 *
 * PNG stuff:
 *  - pCAL:  "used s to identify" [`i']
 *  - gIFt:  no transparency support; rendering order?? what if oFFs in microns?
 */

/*
 * Compilation example (GNU C, command line; fix "zlibpath" as appropriate):
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
 *       link -nologo pngcheck.obj \zlibpath\zlib.lib
 *       [copy pngcheck.exe and zlib.dll to installation directory]
 *
 * zlib info:		http://www.cdrom.com/pub/infozip/zlib/
 * PNG/MNG info:	http://www.cdrom.com/pub/png/  and
 *                      ftp://swrinde.nde.swri.edu/pub/mng/
 * pngcheck sources:	http://www.cdrom.com/pub/png/pngcode.html  or
 *                      ftp://swrinde.nde.swri.edu/pub/png/applications/
 */

#define VERSION "1.99-grr0 of 21 October 1998"

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

int PNG_MNG_check_magic(unsigned char *magic, char *fname, int which);
int PNG_check_chunk_name(char *chunk_name, char *fname);
void make_crc_table(void);
unsigned long update_crc(unsigned long crc, unsigned char *buf, int len);
unsigned long getlong(FILE *fp, char *fname, char *where);
void init_printbuffer(char *fname);
void printbuffer(char *buffer, int size);
void finish_printbuffer(char *fname, char *chunkid);
int keywordlen(char *buffer, int maxsize);
char *getmonth(int m);
void pngcheck(FILE *fp, char *_fname, int searching, FILE *fpOut);

#define BS 32000 /* size of read block for CRC calculation (and zlib) */

/* Mark's macros to extract big-endian short and long ints: */
typedef unsigned char  uch;
typedef unsigned short ush;
typedef unsigned long  ulg;
#define SH(p) ((ush)(uch)((p)[1]) | ((ush)(uch)((p)[0]) << 8))
#define LG(p) ((ulg)(SH((p)+2)) | ((ulg)(SH(p)) << 16))

/* for PNG_MNG_check_magic(): */
#define DO_PNG  0
#define DO_MNG  1

#define set_err(x) error = error < (x) ? (x) : error
#define is_err(x)  ((error > (x) || (!force && error == (x))) ? 1 : 0)
#define no_err(x)  ((error < (x) || (force && error == (x))) ? 1 : 0)

/* Command-line flag variables */
int verbose = 0; /* print chunk info */
int quiet = 0; /* only print error messages */
int printtext = 0; /* print tEXt chunks */
int printpal = 0; /* print PLTE/tRNS/hIST/sPLT contents */
int sevenbit = 0; /* escape characters >=160 */
int force = 0; /* continue even if an occurs (CRC error, etc) */
int search = 0; /* hunt for PNGs in the file... */
int extract = 0; /* ...and extract them to arbitrary file names. */
int mng = 0;        /* it's a MNG instead of a PNG (won't work in pipe) */

int error; /* the current error status */
unsigned char buffer[BS];

/* What the PNG and MNG magic numbers should be */
static unsigned char good_PNG_magic[8] = {137, 80, 78, 71, 13, 10, 26, 10};
static unsigned char good_MNG_magic[8] = {138, 77, 78, 71, 13, 10, 26, 10};

/* table of CRCs of all 8-bit messages */
unsigned long crc_table[256];

/* Flag: has the table been computed? Initially false. */
int crc_table_computed = 0;

#ifdef USE_ZLIB
  int first_idat = 1;   /* flag: is this the first IDAT chunk? */
  int zlib_error = 0;
  unsigned char outbuf[BS];
  z_stream zstrm;
#endif

/* MNG stuff */

static char *inv = "INVALID";

static char *delta_type[] = {
  "full image replacement",
  "block pixel addition",
  "block alpha addition",
  "block pixel replacement",
  "block alpha replacement",
  "no change"
};

static char *framing_mode[] = {
  "don't change",
  "individual images with delays and background restoration (default)",
  "composite frame with final delay and initial background restoration",
  "individual images with delays and initial background restoration",
  "individual images with delays and no background restoration",
  "composite frame with final delay and no background restoration"
};

static char *termination_condition[] = {
  "deterministic",
  "decoder discretion",
  "user discretion",
  "external signal"
};

static char *change_interframe_delay[] = {
  "no change in interframe delay",
  "change interframe delay for next frame",
  "change interframe delay and make default"
};

static char *change_sync_timeout_and_termination[] = {
  "no change in sync timeout and termination",
  "deterministic change in sync timeout and termination for next frame",
  "deterministic change in sync timeout and termination; make default",
  "decoder-discretion change in sync timeout and termination for next frame",
  "decoder-discretion change in sync timeout and termination; make default",
  "user-discretion change in sync timeout and termination for next frame",
  "user-discretion change in sync timeout and termination; make default",
  "change in sync timeout and termination for next frame via signal",
  "change in sync timeout and termination via signal; make default"
};

static char *change_frame_clipping_boundaries[] = {
  "no change in frame clipping boundaries",
  "change frame clipping boundaries for next frame",
  "change frame clipping boundaries and make default"
};

static char *change_sync_id_list[] = {
  "no change in sync ID list",
  "change sync ID list for this frame:",	/* GRR: not next frame?? */
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


/* make the table for a fast crc */
void make_crc_table(void)
{
  int n;

  for (n = 0; n < 256; n++) {
    unsigned long c;
    int k;

    c = (unsigned long)n;
    for (k = 0; k < 8; k++)
      c = c & 1 ? 0xedb88320L ^ (c >> 1):c >> 1;

    crc_table[n] = c;
  }
  crc_table_computed = 1;
}

/* update a running crc with the bytes buf[0..len-1]--the crc should be
   initialized to all 1's, and the transmitted value is the 1's complement
   of the final running crc. */

unsigned long update_crc(unsigned long crc, unsigned char *buf, int len)
{
  unsigned long c = crc;
  unsigned char *p = buf;
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

unsigned long getlong(FILE *fp, char *fname, char *where)
{
  unsigned long res=0;
  int i;

  for(i=0;i<4;i++) {
    int c;

    if((c=fgetc(fp))==EOF) {
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
void putlong(FILE *fpOut, unsigned long ul) {
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

void printbuffer(char *buffer, int size)	/* GRR EBCDIC WARNING */
{
  while(size--) {
    unsigned char c;

    c = *buffer++;

    if((c < ' ' && c != '\t' && c != '\n') ||
       (sevenbit? c > 127:(c >= 127 && c < 160)))
      printf("\\%02X", c);
    else
    if(c == '\\')
      printf("\\\\");
    else
      putchar(c);

    if(c < 32 || (c >= 127 && c < 160)) {
      if(c == '\n') lf=1;
      else if(c == '\r') cr = 1;
      else if(c == '\0') nul = 1;
      else control = 1;
      if(c == 27) esc = 1;
    }
  }
}

void finish_printbuffer(char *fname, char *chunkid)
{
  if(cr) {
    if(lf) {
      printf("%s  %s chunk contains both CR and LF as line terminators\n",
             verbose? "":fname, chunkid);
      set_err(1);
    } else {
      printf("%s  %s chunk contains only CR as line terminator\n",
             verbose? "":fname, chunkid);
      set_err(1);
    }
  }
  if(nul) {
    printf("%s  %s chunk contains null bytes\n", verbose? "":fname, chunkid);
    set_err(1);
  }
  if(control) {
    printf("%s  %s chunk contains control characters%s\n",
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

void print_zlibheader(unsigned long uhead)
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
  long sz;
  unsigned char magic[8];
  char chunkid[5] = {'\0', '\0', '\0', '\0', '\0'};
  int toread;
  int c;
  unsigned long crc, filecrc;
  int ihdr_read = 0, iend_read = 0;
  int mhdr_read = 0, mend_read = 0;
  int dhdr_read = 0;
  int plte_read = 0, idat_read = 0, last_is_idat = 0;
  int have_bkgd = 0, have_chrm = 0, have_gama = 0, have_hist = 0, have_offs = 0;
  int have_phys = 0, have_sbit = 0, have_scal = 0, have_time = 0, have_trns = 0;
  unsigned long zhead = 1; /* 0x10000 indicates both zlib header bytes read. */
  long w = 0, h = 0;
  int bits = 0, ityp = 0, lace = 0, nplte = 0;
  int did_stat = 0;
  struct stat statbuf;
  static int first_file=1;
  static char *type[] = {"grayscale", "undefined type", "RGB", "colormap",
                         "grayscale+alpha", "undefined type", "RGB+alpha"};

  error = 0;

  if (verbose || printtext) {
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
  mng = 0;

  if(!searching) {
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
      } else if ((check = PNG_MNG_check_magic(magic, fname, DO_PNG)) == 0) {
        if (!quiet)
          printf("    this PNG seems to be contained in a MacBinary file\n");
      } else if ((check = PNG_MNG_check_magic(magic, fname, DO_MNG)) == 0) {
        if (!quiet)
          printf("    this MNG seems to be contained in a MacBinary file\n");
      } else {
        if (check == 2)
          printf("    this is not a PNG image or MNG stream\n");
        set_err(2);
      }
    } else if ((check = PNG_MNG_check_magic(magic, fname, DO_PNG)) != 0) {
      if (check == 2) {   /* see if it's a MNG instead */
        if ((check = PNG_MNG_check_magic(magic, fname, DO_MNG)) == 0)
          mng = 1;        /* yup */
        else {
          set_err(2);
          if (check == 2)
            printf("%s  this is neither a PNG image nor a MNG stream\n",
              verbose? "":fname);
        }
      }
    }

    if (is_err(1))
      return;
  }

  while((c=fgetc(fp))!=EOF) {
    ungetc(c, fp);

    if (!mng && iend_read || mng && mend_read) {
      if (searching) /* Start looking again in the current file. */
        return;

      if (!quiet)
        printf("%s  additional data after %cEND chunk\n", verbose? "":fname,
          mng? 'M':'I');

      set_err(1);
      if(!force)
        return;
    }

    sz = getlong(fp, fname, "chunk length");

    if (is_err(2))
      return;

    if(fread(chunkid, 1, 4, fp)!=4) {
      printf("%s  EOF while reading chunk type\n", verbose? ":":fname);
      set_err(3);
      return;
    }

    /* GRR:  add 4-character EBCDIC conversion here (chunkid) */

    chunkid[4] = '\0';

    if (PNG_check_chunk_name(chunkid, fname) != 0) {
      set_err(2);
      if (!force)
        return;
    }

    if(verbose)
      printf("  chunk %s at offset 0x%05lx, length %ld", chunkid,
             ftell(fp)-4, sz);

    if (is_err(1))
      return;

    crc = update_crc(CRCINIT, (unsigned char *)chunkid, 4);

    if ((!mng && !ihdr_read && strcmp(chunkid,"IHDR")!=0) ||
         (mng && !mhdr_read && strcmp(chunkid,"MHDR")!=0))
    {
      printf("%s  first chunk must be %cHDR\n",
             verbose? ":":fname, mng? 'M':'I');
      set_err(1);
      if (!force)
        return;
    }

    toread = (sz > BS)? BS:sz;

    if(fread(buffer, 1, (size_t)toread, fp) != (size_t)toread) {
      printf("%s  EOF while reading %s%sdata\n",
             verbose? ":":fname, verbose? "":chunkid, verbose? "":" ");
      set_err(3);
      return;
    }

    crc = update_crc(crc, (unsigned char *)buffer, toread);

    /*===========================*
     * PNG and MNG header chunks *
     *===========================*/

    /*------* 
     | IHDR | 
     *------*/
    if(strcmp(chunkid, "IHDR") == 0) {
      if (!mng && ihdr_read) {
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
        bits = (unsigned)buffer[8];
        ityp = (unsigned)buffer[9];
        if(ityp > sizeof(type)/sizeof(char*)) {
          ityp = 1; /* avoid out of range array index */
        }
        switch (bits) {
          case 1:
          case 2:
          case 4:
            if (ityp == 2 || ityp == 4 || ityp == 6) {/* RGB or GA or RGBA */
              printf("%s  invalid %sbit depth (%d) for %s image\n",
                     verbose? ":":fname, verbose? "":"IHDR ", bits, type[ityp]);
              set_err(1);
            }
            break;
          case 8:
            break;
          case 16:
            if (ityp == 3) { /* palette */
              printf("%s  invalid %sbit depth (%d) for %s image\n",
                     verbose? ":":fname, verbose? "":"IHDR ", bits, type[ityp]);
              set_err(1);
            }
            break;
          default:
            printf("%s  invalid %sbit depth (%d)\n",
                   verbose? ":":fname, verbose? "":"IHDR ", bits);
            set_err(1);
            break;
        }
        lace = (unsigned)buffer[12];
        switch (ityp) {
          case 2:
            bits *= 3;   /* RGB */
            break;
          case 4:
            bits *= 2;   /* gray+alpha */
            break;
          case 6:
            bits *= 4;   /* RGBA */
            break;
        }
        if (verbose && no_err(1)) {
          printf("\n    %ld x %ld image, %d-bit %s, %sinterlaced\n", w, h,
                 bits, (ityp > 6)? type[1]:type[ityp], lace? "":"non-");
        }
      }
      ihdr_read = 1;
      last_is_idat = 0;
#ifdef USE_ZLIB
      first_idat = 1;  /* flag:  next IDAT will be the first in this file */
      zlib_error = 0;  /* flag:  no zlib errors yet in this file */
#endif

    /*------* 
     | MHDR | 
     *------*/
    } else if(strcmp(chunkid, "MHDR") == 0 && mng) {
      if (mhdr_read) {
        printf("%s  multiple MHDR not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (sz != 12) {
        printf("%s  incorrect %slength\n",
               verbose? ":":fname, verbose? "":"MHDR ");
        set_err(2);
      }
      if (no_err(1)) {
        ulg tps;

        w = LG(buffer);
        h = LG(buffer+4);
        tps = LG(buffer+8);
        if (verbose) {
          printf("\n    %lu x %lu frame size; ", w, h);
          if (tps)
            printf("%lu tick%s per second\n", tps, (tps == 1L)? "" : "s");
          else
            printf("single frame\n");
        }
      }
      mhdr_read = 1;
      last_is_idat = 0;

    /*================================================*
     * PNG chunks (with the exception of IHDR, above) *
     *================================================*/

    /*------* 
     | PLTE | 
     *------*/
    } else if(strcmp(chunkid, "PLTE") == 0) {
      if (!mng && plte_read) {
        printf("%s  multiple PLTE not allowed\n",verbose? ":":fname);
        set_err(1);
      } else if (!mng && idat_read) {
        printf("%s  %smust precede IDAT\n",
               verbose? ":":fname, verbose? "":"PLTE ");
        set_err(1);
      } else if (!mng && have_bkgd) {
        printf("%s  %smust precede bKGD\n",
               verbose? ":":fname, verbose? "":"PLTE ");
        set_err(1);
      } else if (sz < 3 || sz > 768 || sz % 3 != 0) {
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
        if (printpal && !quiet)
          printf("  PLTE chunk");
        if (verbose || (printpal && !quiet))
          printf(": %d palette entries\n", nplte);
        if (printpal) {
          char *spc;
          int i, j;

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
      last_is_idat = 0;

    /*------* 
     | IDAT | 
     *------*/
    } else if(strcmp(chunkid, "IDAT") == 0) {
      /* GRR:  need to check for consecutive IDATs within MNG segments */
      if (idat_read && !last_is_idat) {
        if (mng) {  /* reset things (GRR SEMI-HACK:  check for segments!) */
          idat_read = 0;
          zhead = 1;
          if (verbose)
            printf("\n");
        } else {
          printf("%s  IDAT chunks must be consecutive\n",verbose? ":":fname);
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
        idat_read = sz > 0 ? sz:-1;
      else if (idat_read < 10)
        idat_read += sz > 10 ? 10:sz;

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
        static unsigned char *p;   /* always points to next filter byte */
        static int cur_y, cur_pass, cur_xoff, cur_yoff, cur_xskip, cur_yskip;
        static long cur_width, cur_linebytes;
        static long numfilt, numfilt_this_block, numfilt_total;
        unsigned char *eod;
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
          cur_width = (w + cur_xskip - 1) / cur_xskip;     /* round up */
          cur_linebytes = ((cur_width*bits + 7) >> 3) + 1; /* round up + fltr */
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

        printf("    zlib line filters (0 none, 1 sub, 2 up, 3 avg, 4 paeth):\n"
          "     ");
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
            printf(" %1d", (int)p[0]);
            ++numfilt;
            if (++numfilt_this_block % 25 == 0)
              printf("\n     ");
            p += cur_linebytes;
            cur_y += cur_yskip;
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
                cur_linebytes = ((cur_width*bits + 7) >> 3) + 1;
              }
            } else if (cur_y >= h) {
              inflateEnd(&zstrm);     /* we're all done */
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

    /*------* 
     | IEND | 
     *------*/
    } else if(strcmp(chunkid, "IEND") == 0) {
      if (!mng && iend_read) {
        printf("%s  multiple IEND not allowed\n",verbose? ":":fname);
        set_err(1);
      } else if (sz != 0) {
        printf("%s  incorrect %slength\n",
               verbose? ":":fname, verbose? "":"IEND ");
        set_err(1);
      } else if (idat_read < 10) {
        printf("%s  not enough IDAT data\n", verbose? ":":fname);
      } else if (verbose) {
        printf("\n");
      }
      iend_read = 1;
      last_is_idat = 0;

    /*------* 
     | bKGD | 
     *------*/
    } else if(strcmp(chunkid, "bKGD") == 0) {
      if (!mng && have_bkgd) {
        printf("%s  multiple bKGD not allowed\n",verbose? ":":fname);
        set_err(1);
      } else if (idat_read) {
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
      last_is_idat = 0;

    /*------* 
     | cHRM | 
     *------*/
    } else if(strcmp(chunkid, "cHRM") == 0) {
      if (!mng && have_chrm) {
        printf("%s  multiple cHRM not allowed\n",verbose? ":":fname);
        set_err(1);
      } else if (!mng && plte_read) {
        printf("%s  %smust precede PLTE\n",
               verbose? ":":fname, verbose? "":"cHRM ");
        set_err(1);
      } else if (!mng && idat_read) {
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
      last_is_idat = 0;

    /*------* 
     | gAMA | 
     *------*/
    } else if(strcmp(chunkid, "gAMA") == 0) {
      if (!mng && have_gama) {
        printf("%s  multiple gAMA not allowed\n",verbose? ":":fname);
        set_err(1);
      } else if (!mng && idat_read) {
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
      last_is_idat = 0;

    /*------* 
     | hIST | 
     *------*/
    } else if(strcmp(chunkid, "hIST") == 0) {
      if (!mng && have_hist) {
        printf("%s  multiple hIST not allowed\n",verbose? ":":fname);
        set_err(1);
      } else if (!plte_read) {
        printf("%s  %smust follow PLTE\n",
               verbose? ":":fname, verbose? "":"hIST ");
        set_err(1);
      } else if (idat_read) {
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
        printf(": %ld histogram entries\n", sz / 2);
      }
      if (printpal && no_err(1)) {
        char *spc;
        int i, j;

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
      last_is_idat = 0;

    /*------* 
     | oFFs | 
     *------*/
    } else if(strcmp(chunkid, "oFFs") == 0) {
      if (!mng && have_offs) {
        printf("%s  multiple oFFs not allowed\n",verbose? ":":fname);
        set_err(1);
      } else if (idat_read) {
        printf("%s  %smust precede IDAT\n",
               verbose? ":":fname, verbose? "":"oFFs ");
        set_err(1);
      } else if (sz != 9) {
        printf("%s  incorrect %slength\n",
               verbose? ":":fname, verbose? "":"oFFs ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        printf(": %ldx%ld %s offset\n", LG(buffer), LG(buffer +4),
               buffer[8]==0 ? "pixels":
               buffer[8]==1 ? "micrometers":"unknown unit");
      }
      have_offs = 1;
      last_is_idat = 0;

    /*------* 
     | pHYs | 
     *------*/
    } else if(strcmp(chunkid, "pHYs") == 0) {
      if (!mng && have_phys) {
        printf("%s  multiple pHYS not allowed\n",verbose? ":":fname);
        set_err(1);
      } else if (idat_read) {
        printf("%s  %smust precede IDAT\n",
               verbose? ":":fname, verbose? "":"pHYS ");
        set_err(1);
      } else if (sz != 9) {
        printf("%s  incorrect %slength\n",
               verbose? ":":fname, verbose? "":"pHYS ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        printf(": %ldx%ld pixels/%s\n", LG(buffer), LG(buffer +4),
               buffer[8]==0 ? "unit":buffer[8]==1 ? "meter":"unknown unit");
      }
      have_phys = 1;
      last_is_idat = 0;

    /*------* 
     | sBIT | 
     *------*/
    } else if(strcmp(chunkid, "sBIT") == 0) {
      int maxbits;

      maxbits = ityp == 3 ? 8:bits;

      if (!mng && have_sbit) {
        printf("%s  multiple sBIT not allowed\n",verbose? ":":fname);
        set_err(1);
      } else if (plte_read) {
        printf("%s  %smust precede PLTE\n",
               verbose? ":":fname, verbose? "":"sBIT ");
        set_err(1);
      } else if (idat_read) {
        printf("%s  %smust precede IDAT\n",
               verbose? ":":fname, verbose? "":"sBIT ");
        set_err(1);
      }
      switch (ityp) {
        case 0:
          if (sz != 1) {
            printf("%s  incorrect %slength\n",
                   verbose? ":":fname, verbose? "":"sBIT ");
            set_err(2);
          } else if (buffer[0] == 0 || buffer[0] > maxbits) {
            printf("%s  %d %sgrey bits not valid for %dbit/sample image\n",
                   verbose? ":":fname, buffer[0], verbose? "":"sBIT ", maxbits);
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
      last_is_idat = 0;

    /*------* 
     | sCAL | 
     *------*/
    } else if(strcmp(chunkid, "sCAL") == 0) {
      if (!mng && have_scal) {
        printf("%s  multiple sCAL not allowed\n",verbose? ":":fname);
        set_err(1);
      } else if (idat_read) {
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
      last_is_idat = 0;

    /*------* 
     | sPLT | 
     *------*/
    } else if(strcmp(chunkid, "sPLT") == 0) {
      if (idat_read) {
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
            printf(": %ld palette/histogram entries\n", nsplt);
            printf("    sample depth = %u bits, palette name = ", bps);
            init_printbuffer(fname);
            printbuffer(buffer, name_len);
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
                printf("%s%3ld:  (%3u,%3u,%3u,%3u) = (0x%02x,0x%02x,0x%02x,0x%02x)  freq = %u\n",
                       spc, i,
                       buffer[j], buffer[j+1], buffer[j+2], buffer[j+3],
                       buffer[j], buffer[j+1], buffer[j+2], buffer[j+3],
                       SH(buffer+j+4));
            } else {
              for (i = 0L;  i < nsplt;  ++i, j += 10L)
                printf("%s%5ld:  (%5u,%5u,%5u,%5u) = (%04x,%04x,%04x,%04x)  freq = %u\n",
                       spc, i, SH(buffer+j), SH(buffer+j+2), SH(buffer+j+4),
                       SH(buffer+j+6), SH(buffer+j), SH(buffer+j+2),
                       SH(buffer+j+4), SH(buffer+j+6), SH(buffer+j+8));
            }
          }
        }
      }
      last_is_idat = 0;

    /*------*  *------* 
     | tEXt |  | zTXt | 
     *------*  *------*/
    } else if(strcmp(chunkid, "tEXt") == 0 || strcmp(chunkid, "zTXt") == 0) {
      int key_len;

      key_len = keywordlen(buffer, toread);
      if(key_len == 0) {
        printf("%s  zero length %s%skeyword\n",
               verbose? ":":fname, verbose? "":chunkid, verbose? "":" ");
        set_err(1);
      } else if(key_len > 79) {
        printf("%s  %s keyword longer than 79 characters\n",
               verbose? ":":fname, verbose? "":chunkid);
        set_err(1);
      }
      if (verbose && no_err(1) && buffer[0] == ' ') {
        printf("%s  %s keyword has leading space(s)\n",
               verbose? ":":fname, verbose? "":chunkid);
        set_err(1);
      }
      if (verbose && no_err(1) && buffer[key_len - 1] == ' ') {
        printf("%s  %s keyword has trailing space(s)\n",
               verbose? ":":fname, verbose? "":chunkid);
        set_err(1);
      }
      if (no_err(1)) {
        int i, prev_space = 0;

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
      if (verbose && no_err(1)) {
        int i;

        for(i = 0; i < key_len; i++) {
          if((unsigned char)buffer[i]<32 ||
             ((unsigned char)buffer[i]>=127 && (unsigned char)buffer[i]<161)) {
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
          printbuffer(buffer, key_len);
        }
        if (printtext) {
          printf(verbose? "\n":":");
          if (strcmp(chunkid, "tEXt") == 0)
            printbuffer(buffer + key_len + 1, toread - key_len - 1);
          else
            printf("(compressed %s text)", chunkid);

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
      last_is_idat = 0;

    /*------* 
     | tIME | 
     *------*/
    } else if(strcmp(chunkid, "tIME") == 0) {
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
      last_is_idat = 0;

    /*------* 
     | tRNS | 
     *------*/
    } else if(strcmp(chunkid, "tRNS") == 0) {
      if (!mng && have_trns) {
        printf("%s  multiple tRNS not allowed\n", verbose? ":":fname);
        set_err(1);
      } else if (ityp == 3 && !plte_read) {
        printf("%s  %smust follow PLTE\n",
          verbose? ":":fname, verbose? "":"tRNS ");
        set_err(1);
      } else if (idat_read) {
        printf("%s  %smust precede IDAT\n",
               verbose? ":":fname, verbose? "":"tRNS ");
        set_err(1);
      }
      switch (ityp) {
        case 0:
          if (sz != 2) {
            printf("%s  incorrect %slength for %s image\n",
                   verbose? ":":fname, verbose? "":"tRNS ",type[ityp]);
            set_err(2);
          } else if (verbose && no_err(1)) {
            printf(": gray = %d\n", SH(buffer));
          }
          break;
        case 2:
          if (sz != 6) {
            printf("%s  incorrect %slength for %s image\n",
                   verbose? ":":fname, verbose? "":"tRNS ",type[ityp]);
            set_err(2);
          } else if (verbose && no_err(1)) {
            printf(": red = %d green = %d blue = %d\n", SH(buffer),
                   SH(buffer+2), SH(buffer+4));
          }
          break;
        case 3:
          if (sz > nplte) {
            printf("%s  incorrect %slength for %s image\n",
                   verbose? ":":fname, verbose? "":"tRNS ", type[ityp]);
            set_err(2);

          } else if ((verbose || (printpal && !quiet)) && no_err(1)) {
            if (printpal && !quiet)
              printf("  tRNS chunk");
            printf(": %ld transparency entries\n", sz);
          }
          if (printpal && no_err(1)) {
            char *spc;
            int i;

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
                 verbose? ":":fname, verbose? "":"tRNS ", type[ityp]);
          set_err(1);
          break;
      }
      have_trns = 1;
      last_is_idat = 0;
    }

    /*================================================*
     * MNG chunks (with the exception of MHDR, above) *
     *================================================*/

    /*------* 
     | DHDR | 
     *------*/
    else if(strcmp(chunkid, "DHDR") == 0) {	/* DELTA-PNG */
      if (sz != 4 && sz != 12 && sz != 20) {
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
      last_is_idat = 0;

    /*------* 
     | FRAM | 
     *------*/
    } else if(strcmp(chunkid, "FRAM") == 0) {
      if (sz == 0 && verbose) {
        printf(":  empty\n");
      } else if (verbose) {
        uch fmode = buffer[0];

        printf("\n    %s\n", (fmode < sizeof(framing_mode)/sizeof(char *))?
          framing_mode[fmode] : inv);
        if (sz > 1) {
          uch *p = buffer+1;
          int bytes_left;

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
          } else
            ++p;  /* skip over null */
          bytes_left = sz - (p-buffer);
          if (bytes_left < 4) {
            printf("%s  incorrect %slength\n",
              verbose? ":":fname, verbose? "":"FRAM ");
            set_err(2);
          } else {
            uch cid = *p++;	/* change_interframe_delay */
            uch cstt = *p++;	/* change_sync_timeout_and_termination */
            uch cfcb = *p++;	/* change_frame_clipping_boundaries */
            uch csil = *p++;	/* change_sync_id_list */

            if (cid > 2 || cstt > 8 || cfcb > 2 || csil > 2) {
              printf("%s  invalid %schange flags\n",
                verbose? ":":fname, verbose? "":"FRAM ");
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
              printf("    %s\n", change_sync_timeout_and_termination[cstt]);
              /* GRR:  need real error-checking here: */
              if (cstt && bytes_left >= 4) {
                ulg val = LG(p);

                if (val == 0x7fffffffL)
                  printf("      new timeout = infinite\n");
                else
                  printf("      new timeout = %lu tick%s\n", val, (val == 1L)?
                    "" : "s");
                p += 4;
                bytes_left -= 4;
              }
              printf("    %s\n", change_frame_clipping_boundaries[cfcb]);
              /* GRR:  need real error-checking here: */
              if (cfcb && bytes_left >= 17) {
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
      last_is_idat = 0;

    /*------* 
     | SAVE | 
     *------*/
    } else if(strcmp(chunkid, "SAVE") == 0) {
      if (sz > 0 && verbose) {
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
      last_is_idat = 0;

    /*------* 
     | SEEK | 
     *------*/
    } else if(strcmp(chunkid, "SEEK") == 0) {
      if (sz > 0 && verbose) {
        printf("\n    ");
        init_printbuffer(fname);
        printbuffer(buffer, sz);
        finish_printbuffer(fname, chunkid);
      }
      last_is_idat = 0;

    /*------* 
     | nEED | 
     *------*/
    } else if(strcmp(chunkid, "nEED") == 0) {
      if (sz > 0 && verbose) {
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
      last_is_idat = 0;

    /*------* 
     | DEFI | 
     *------*/
    } else if(strcmp(chunkid, "DEFI") == 0) {
      if (sz != 2 && sz != 3 && sz != 4 && sz != 12 && sz != 28) {
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
      last_is_idat = 0;

    /*------* 
     | BACK | 
     *------*/
    } else if(strcmp(chunkid, "BACK") == 0) {
      if (sz != 6 && sz != 7) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"BACK ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        printf("\n    red = %u, green = %u, blue = %u (%s)\n",
          SH(buffer), SH(buffer+2), SH(buffer+4), (sz > 6 && buffer[6])?
          "mandatory":"advisory");
      }
      last_is_idat = 0;

    /*------* 
     | MOVE | 
     *------*/
    } else if(strcmp(chunkid, "MOVE") == 0) {
      if (sz != 13) {
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
      last_is_idat = 0;

    /*------* 
     | CLON | 
     *------*/
    } else if(strcmp(chunkid, "CLON") == 0) {
      if (sz != 4 && sz != 5 && sz != 6 && sz != 7 && sz != 16) {
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
      last_is_idat = 0;

    /*------* 
     | SHOW | 
     *------*/
    } else if(strcmp(chunkid, "SHOW") == 0) {
      if (sz != 0 && sz != 2 && sz != 4 && sz != 5) {
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
      last_is_idat = 0;

    /*------* 
     | CLIP | 
     *------*/
    } else if(strcmp(chunkid, "CLIP") == 0) {
      if (sz != 21) {
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
      last_is_idat = 0;

    /*------* 
     | LOOP | 
     *------*/
    } else if(strcmp(chunkid, "LOOP") == 0) {
      if (sz != 6 && sz != 14 && sz != 18) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"LOOP ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        printf(":  nest level = %u\n    count = %lu, termination = %s\n",
          (unsigned)(buffer[0]), LG(buffer+2),
          termination_condition[buffer[1] & 0x3]);
          /* GRR:  not checking for valid buffer[1] values */
        if (sz > 6) {
          printf("    iteration min = %lu, max = %lu",
            LG(buffer+6), LG(buffer+10));
          if (sz > 14) {
            printf(", signal number = %lu\n", LG(buffer+14));
          } else
            printf("\n");
        }
      }
      last_is_idat = 0;

    /*------* 
     | ENDL | 
     *------*/
    } else if(strcmp(chunkid, "ENDL") == 0) {
      if (sz != 1) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"ENDL ");
        set_err(2);
      }
      if (verbose && no_err(1))
        printf(":  nest level = %u\n", (unsigned)(buffer[0]));
      last_is_idat = 0;

    /*------* 
     | PROM | 
     *------*/
    } else if(strcmp(chunkid, "PROM") == 0) {
      if (sz != 3) {
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
      last_is_idat = 0;

    /*------* 
     | fPRI | 
     *------*/
    } else if(strcmp(chunkid, "fPRI") == 0) {
      if (sz != 2) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"fPRI ");
        set_err(2);
      }
      if (verbose && no_err(1))
        printf(":  %spriority = %u\n", buffer[0]? "delta " : "",
          (unsigned)(buffer[1]));
      last_is_idat = 0;

    /*------* 
     | eXPI | 
     *------*/
    } else if(strcmp(chunkid, "eXPI") == 0) {
      if (sz <= 2) {
        printf("%s  incorrect %slength\n",
          verbose? ":":fname, verbose? "":"eXPI ");
        set_err(2);
      }
      if (verbose && no_err(1)) {
        printf("\n    snapshot ID = %u, snapshot name = %.*s\n", SH(buffer),
          sz-2, buffer+2);	/* GRR EBCDIC WARNING */
      }
      last_is_idat = 0;

    /*------* 
     | MEND | 
     *------*/
    } else if(strcmp(chunkid, "MEND") == 0) {
      if (mend_read) {
        printf("%s  multiple MEND not allowed\n",verbose? ":":fname);
        set_err(1);
      } else if (sz != 0) {
        printf("%s  incorrect %slength\n",
               verbose? ":":fname, verbose? "":"MEND ");
        set_err(1);
      } else if (verbose) {
        printf("\n");
      }
      mend_read = 1;
      last_is_idat = 0;

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
        printf(": unknown %s%s%s%s chunk\n",
               chunkid[0] & 0x20 ? "ancillary ":"critical ",
               chunkid[1] & 0x20 ? "private ":"",
               chunkid[2] & 0x20 ? "reserved-bit-set ":"",
               chunkid[3] & 0x20 ? "safe-to-copy":"unsafe-to-copy");
      }
      last_is_idat = 0;
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

        if(data_read != toread) {
          printf("%s  EOF while reading %s%sdata\n",
                 verbose? ":":fname, verbose? "":chunkid, verbose? "":" ");
          set_err(3);
          return;
        }

        crc = update_crc(crc, (unsigned char *)buffer, toread);
      }

      filecrc = getlong(fp, fname, "crc value");

      if (is_err(2))
        return;

      if(filecrc != CRCCOMPL(crc)) {
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

  if(!mng && !iend_read || mng && !mend_read) {
    printf("%s  file doesn't end with a%sEND chunk\n", verbose? "":fname,
      mng? " M":"n I");
    set_err(1);
    return;
  }

  /* GRR 970621: print compression ratio based on file size vs. byte-packed
   * raw data size.  Arguably it might be fairer to compare against the size
   * of the unadorned, compressed data, but since PNG is a package deal... */

  if (error == 0) {
    char *sgn = "";
    int cfactor;

    if (!did_stat) {
      stat(fname, &statbuf);   /* already know file exists */
    }

    /* uncompressed size (bytes), compressed size => returns 10*ratio (%) */
    if ((cfactor = ratio((ulg)(h*((w*bits+7)>>3)), statbuf.st_size)) < 0) {
      sgn = "-";
      cfactor = -cfactor;
    }

    if (verbose) {  /* already printed IHDR info */
      printf("No errors detected in %s (%s%d.%d%% compression).\n", fname, sgn,
        cfactor/10, cfactor%10);
    } else if (!quiet) {
      printf("No errors detected in %s (%ldx%ld, %d-bit %s, %sinterlaced,"
             " %s%d.%d%%).\n", fname, w, h, bits,
             (ityp > 6)? type[1]:type[ityp], lace? "":"non-",
             sgn, cfactor/10, cfactor%10);
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
  /* Use this if file name length is restricted. */
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
      case 'v':
        ++verbose;  /* verbose == 2 means decode IDATs and print filter info */
        quiet=0;
        i++;
        break;
      case 'q':
        verbose=0;
        quiet=1;
        i++;
        break;
      case 't':
        printtext=1;
        i++;
        break;
      case '7':
        printtext=1;
        sevenbit=1;
        i++;
        break;
      case 'p':
        printpal=1;
        i++;
        break;
      case 'f':
        force=1;
        i++;
        break;
      case 's':
        search=1;
        i++;
        break;
      case 'x':
        search=extract=1;
        i++;
        break;
      case 'h':
        goto usage;
        break;
      default:
        fprintf(stderr, "unknown option %c\n", argv[1][i]);
        set_err(1);
        goto usage;
    }
  }

  if(argc == 1) {
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
      fprintf(stderr, "   -p  print contents of PLTE, tRNS, hIST and sPLT chunks (can be used with -q)\n");
      fprintf(stderr, "   -f  force continuation even after major errors\n");
      fprintf(stderr, "   -s  search for PNGs within another file\n");
      fprintf(stderr, "   -x  search for PNGs and extract them when found\n");
      fprintf(stderr, "\nNote:  MNG support is incomplete.  Based on MNG Draft 43.\n");
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

/* PNG_MNG_check_magic()
 *
 * Check the magic numbers in 8-byte buffer at the beginning of
 * a (possible) PNG or MNG file.
 *
 * by Alexander Lehmann, Glenn Randers-Pehrson and Greg Roelofs
 *
 * This is free software; you can redistribute it and/or modify it
 * without any restrictions.
 *
 */

int PNG_MNG_check_magic(unsigned char *magic, char *fname, int which)
{
  int i;
  unsigned char *good_magic = which? good_MNG_magic : good_PNG_magic;

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
      printf("%s  file is CORRUPTED by text conversion.\n", fname);
      return 1;
    }

    printf("  file is CORRUPTED.\n");

    /* This coding derived from Alexander Lehmann's checkpng code   */
    if(strncmp((char *)&magic[4], "\012\032", 2) == 0)
      printf("  It seems to have suffered DOS->Unix conversion\n");

    else if(strncmp((char *)&magic[4], "\015\032", 2) == 0)
      printf("  It seems to have suffered DOS->Mac conversion\n");

    else if(strncmp((char *)&magic[4], "\015\015\032", 3) == 0)
      printf("  It seems to have suffered Unix->Mac conversion\n");

    else if(strncmp((char *)&magic[4], "\012\012\032", 3) == 0)
      printf("  It seems to have suffered Mac->Unix conversion\n");

    else if(strncmp((char *)&magic[4], "\012\012", 2) == 0)
      printf("  It seems to have suffered DOS->Unix conversion\n");

    else if(strncmp((char *)&magic[4], "\015\015\012\032", 4) == 0)
      printf("  It seems to have suffered Unix->DOS conversion\n");

    else if(strncmp((char *)&magic[4], "\015\012\032\015", 4) == 0)
      printf("  It seems to have suffered Unix->DOS conversion\n");

    else if(strncmp((char *)&magic[4], "\015\012\012", 3) == 0)
      printf("  It seems to have suffered DOS EOF conversion\n");

    else if(strncmp((char *)&magic[4], "\015\012\032\012", 4) != 0)
      printf("  It seems to have suffered EOL conversion\n");

    if (magic[0] == 9)
      printf("  It was probably transmitted through a 7-bit channel\n");
    else if(magic[0] != good_magic[0])
      printf("  It was probably transmitted in text mode\n");

    return 1;
  }

  return 0;
}

int PNG_check_chunk_name(char *chunk_name, char *fname)
{
  if(!isalpha(chunk_name[0]) || !isalpha(chunk_name[1]) ||
     !isalpha(chunk_name[2]) || !isalpha(chunk_name[3])) {
    printf("%s%schunk name %02x %02x %02x %02x doesn't comply to naming rules\n",
           verbose? "":fname, verbose? "":": ",
           chunk_name[0], chunk_name[1], chunk_name[2], chunk_name[3]);
    set_err(2);
    return (1);
  }

  return (0);
}
