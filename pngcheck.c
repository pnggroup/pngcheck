/*
 * Authenticate the structure of a PNG file and dump info about it if desired.
 *
 * This program checks the PNG identifier with conversion checks,
 * the file structure and the chunk CRCs. In addition, relations and contents
 * of most defined chunks are checked, except for decompression of the IDAT
 * chunks.
 *
 * Started by Alexander Lehmann <alex@hal.rhein-main.de> and subsequently
 * extended by people as listed below.
 *
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
 * 96.12.31 JB: add decoding of the Zlib header from the first IDAT chunk (16 bit
 *              header code in first two bytes, see print_zlibheader).
 */

/* NOTE: Base version is 1.97 from swrinde */
#define VERSION "1.97jb of 31 December 1996"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

int PNG_check_magic(unsigned char *magic, char *fname);
int PNG_check_chunk_name(char *chunk_name, char *fname);
void make_crc_table(void);
unsigned long update_crc(unsigned long crc, unsigned char *buf, int len);
unsigned long getlong(FILE *fp, char *fname);
void init_printbuffer(char *fname);
void printbuffer(char *buffer, int size, int printtext);
void finish_printbuffer(char *fname);
int keywordlen(char *buffer, int maxsize);
char *getmonth(int m);
void pngcheck(FILE *fp, char *_fname, int searching, FILE *fpOut);

#define BS 32000 /* size of read block for CRC calculation */

/* Mark's macros to extract big-endian short and long ints: */
typedef unsigned char  uch;
typedef unsigned short ush;
typedef unsigned long  ulg;
#define SH(p) ((ush)(uch)((p)[1]) | ((ush)(uch)((p)[0]) << 8))
#define LG(p) ((ulg)(SH((p)+2)) | ((ulg)(SH(p)) << 16))

int verbose; /* print chunk info */
int quiet; /* only print error messages */
int printtext; /* print tEXt chunks */
int printpal; /* print PLTE contents */
int sevenbit; /* escape characters >=160 */
int error; /* an error already occured, but we are continueing */
int force; /* continue even if a really bad error occurs (CRC error, etc) */
int search; /* Hunt for PNGs in the file. */
int extract; /* And extract them to arbitrary file names. */
unsigned char buffer[BS];

/* table of crc's of all 8-bit messages */
unsigned long crc_table[256];

/* Flag: has the table been computed? Initially false. */
int crc_table_computed = 0;

/* make the table for a fast crc */
void make_crc_table(void)
{
  int n;

  for (n = 0; n < 256; n++) {
    unsigned long c;
    int k;

    c = (unsigned long)n;
    for (k = 0; k < 8; k++)
      c = c & 1 ? 0xedb88320L ^ (c >> 1) : c >> 1;

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

unsigned long getlong(FILE *fp, char *fname)
{
  unsigned long res=0;
  int i;

  for(i=0;i<4;i++) {
    int c;

    if((c=fgetc(fp))==EOF) {
      printf("%s: EOF while reading 4 bytes value\n", fname);
      error=2;
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

void printbuffer(char *buffer, int size, int printtext)
{
  while(size--) {
    unsigned char c;

    c = *buffer++;
    if(printtext) {
      if((c < ' ' && c != '\t' && c != '\n') ||
         (sevenbit ? c > 127 : (c >= 127 && c < 160)))
        printf("\\%02X", c);
      else
      if(c == '\\')
        printf("\\\\");
      else
        putchar(c);
    }

    if(c < 32 || (c >= 127 && c < 160)) {
      if(c == '\n') lf=1;
      else if(c == '\r') cr = 1;
      else if(c == '\0') nul = 1;
      else control = 1;
      if(c == 27) esc = 1;
    }
  }
}

void finish_printbuffer(char *fname)
{
  if(cr)
    if(lf) {
      printf("%s  text chunk contains both CR and LF as line terminators\n",
             verbose? "":fname);
      error=1;
    } else {
      printf("%s  text chunk contains only CR as line terminator\n",
             verbose? "":fname);
      error=1;
    }
  if(nul) {
    printf("%s  text chunk contains null bytes\n", verbose? "":fname);
    error=1;
  }
  if(control) {
    printf("%s  text chunk contains control characters%s\n",
           verbose? "":fname, esc ? " including Escape":"");
    error=1;
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

void print_zlibheader(unsigned long uhead)
{
  /* See the code in zlib deflate.c which writes out the header when s->status is
	  INIT_STATE.  In fact this code is based on the zlib specification in
	  RFC 1950, i.e. ftp://ds.internic.net/rfc/rfc1950.txt with the implicit
	  assumption that the zlib header *is* written (it always should be inside
	  a valid PNG file)  The variable names are taken, verbatim, from the RFC
	  */
  unsigned int CM = (uhead & 0xf00) >> 8;
  unsigned int CINFO = (uhead & 0xf000) >> 12;
  unsigned int FDICT = (uhead & 0x20) >> 5;
  unsigned int FLEVEL = (uhead & 0xc0) >> 6;

  /* The following is awkable. */
  printf("    ZLIB %ld %s %d %d %d %d\n", uhead,
			uhead % 31 == 0 ? "OK" : "XX",
			CM, CINFO, FDICT, FLEVEL);
  /* The following is (maybe) readable, note that we only get 4 level
	  values from the header so the possible input range is output (based
     on the code in deflate.c).  Also level==0 seems to result in an
     overflow in the initializer, so 3 gets stored. */
  if (CM  == 8)
	  printf("    deflate(%d ln2-bits%s) compression level %d-%d%s\n",
	  CINFO+8, CINFO>7?" INVALID":"",
	  (FLEVEL<<1)+1, FLEVEL > 2 ? 0 : (FLEVEL<<1)+2,
	  FDICT ? " preset dictionary" : "");
  else if (CM == 15)
     /* probably an experiment! */
	  printf("    RESERVED(%d) compression\n", CINFO);
  else
	  printf("    ZLIB(%d, %d) compression (INVALID)\n", CM, CINFO);
}

void pngcheck(FILE *fp, char *fname, int searching, FILE *fpOut)
{
  long sz;
  unsigned char magic[8];
  char chunkid[5] = {'\0', '\0', '\0', '\0', '\0'};
  int toread;
  int c;
  unsigned long crc, filecrc;
  int ihdr_read = 0, plte_read = 0, idat_read = 0, last_is_idat = 0;
  int iend_read = 0;
  int have_bkgd = 0, have_chrm = 0, have_gama = 0, have_hist = 0, have_offs = 0;
  int have_phys = 0, have_sbit = 0, have_scal = 0, have_time = 0, have_trns = 0;
  unsigned long zhead = 1; /* 0x10000 indicates both Zlib header bytes read. */
  long w = 0, h = 0;
  int bits = 0, ityp = 0, lace = 0, nplte = 0;
  static int first_file=1;
  static char *type[] = {"grayscale", "undefined type", "RGB", "colormap",
                         "grayscale+alpha", "undefined type", "RGB+alpha"};

  error = 0;

  if(verbose || printtext) {
    printf("%sFile: %s\n", first_file?"\n":"", fname);
  }

  first_file = 0;

  if(!searching && fread(magic, 1, 8, fp)!=8) {
    printf("%s  cannot read PNG header\n", verbose? "":fname);
    return;
  }

  if (!searching && PNG_check_magic(magic, fname) != 0) {
    /* maybe it's a MacBinary file */

    if(magic[0]==0 && magic[1]>0 && magic[1]<=64 && magic[2]!=0) {
      if (!quiet)
        printf("%s  (trying to skip MacBinary header)\n", verbose? "":fname);
      if(fread(buffer, 1, 120, fp)==120 &&
         fread(magic, 1, 8, fp)==8 &&
         PNG_check_magic(magic, fname) == 0) {
        if (!quiet)
          printf("%s  this PNG seems to be contained in a MacBinary file\n",
                 verbose? "":fname);
      } else
        return;
    } else
      return;
  }

  while((c=fgetc(fp))!=EOF) {
    ungetc(c, fp);

    if(iend_read) {
      if (!quiet)
        printf("%s  additional data after IEND chunk\n", verbose? "":fname);
      if(force)
        error = 1;
      else
        return;
    }

    sz = getlong(fp, fname);

    if(fread(chunkid, 1, 4, fp)!=4) {
      printf("%s  EOF while reading chunk type\n", verbose? "":fname);
      return;
    }

    chunkid[4] = 0;

    if (PNG_check_chunk_name(chunkid, fname) != 0) {
      if (force)
        error = 2;
      else
        return;
    }

    if(verbose)
      printf("  chunk %s at offset 0x%05lx, length %ld", chunkid,
             ftell(fp)-4, sz);

	 if (fpOut != NULL) {
	   putlong(fpOut, sz);
		(void)fwrite(chunkid, 1, 4, fpOut);
	 }

    crc = update_crc(CRCINIT, (unsigned char *)chunkid, 4);

    if(!ihdr_read && strcmp(chunkid,"IHDR")!=0) {
        printf("\n%s  file doesn't start with a IHDR chunk\n",
               verbose? "":fname);
        if (force)
          error = 2;
        else
          return;
    }

    toread = (sz > BS) ? BS : sz;

    if(fread(buffer, 1, toread, fp)!=toread) {
      printf("\n%s  EOF while reading chunk data (%s)\n",
             verbose ? "":fname, chunkid);
      return;
    }
	 if (fpOut != NULL) (void)fwrite(buffer, 1, toread, fpOut);

    crc = update_crc(crc, (unsigned char *)buffer, toread);

    if(strcmp(chunkid, "IHDR") == 0) {
      if (ihdr_read) {
        printf("%s  multiple IHDR not allowed\n", verbose?":":fname);
        error = 1;
      } else if (sz != 13) {
        printf("%s  incorrect %slength\n",
               verbose?":":fname, verbose?"":"IHDR ");
        error = 2;
      }
      if (error == 0 || (error == 1 && force)) {
        w = LG(buffer);
        h = LG(buffer+4);
        if (w == 0 || h == 0) {
          printf("%s  invalid %simage dimensions (%dx%d)\n",
                 verbose?":":fname, verbose?"":"IHDR ", w, h);
          error = 1;
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
                     verbose?":":fname, verbose?"":"IHDR ", bits, type[ityp]);
              error = 1;
            }
            break;
          case 8:
            break;
          case 16:
            if (ityp == 3) { /* palette */
              printf("%s  invalid %sbit depth (%d) for %s image\n",
                     verbose?":":fname, verbose?"":"IHDR ",bits, type[ityp]);
              error = 1;
            }
            break;
          default:
            printf("%s  invalid %sbit depth (%d)\n",
                   verbose?":":fname, verbose?"":"IHDR ", bits);
            error = 1;
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
        if (verbose && (error == 0 || (error == 1 && force))) {
          printf("\n  %ld x %ld image, %d-bit %s, %sinterlaced\n", w, h,
                 bits, (ityp > 6)? type[1]:type[ityp], lace? "":"non-");
        }
      }
      ihdr_read = 1;
      last_is_idat = 0;
    } else if(strcmp(chunkid, "PLTE") == 0) {
      if (plte_read) {
        printf("%s  multiple PLTE not allowed\n",verbose?":":fname);
        error = 1;
      } else if (idat_read) {
        printf("%s  %smust precede IDAT\n",
               verbose?":":fname, verbose?"":"PLTE ");
        error = 1;
      } else if (have_bkgd) {
        printf("%s  %smust precede bKGD\n",
               verbose?":":fname, verbose?"":"PLTE ");
        error = 1;
      } else if (sz < 3 || sz > 768 || sz % 3 != 0) {
        printf("%s  invalid number of %sentries (%g)\n",
               verbose?":":fname, verbose?"":"PLTE ",(double)sz / 3);
        error = 2;
      }
      else if (printpal && !verbose)
        printf("\n");

      if (error == 0 || (error == 1 && force)) {
        nplte = sz / 3;
        if (verbose)
          printf(": %d palette entries\n", nplte);

        if (printpal) {
          char *spc;
          int i, j;

          if (nplte < 10)
            spc = "";
          else if (nplte < 100)
            spc = " ";
          else
            spc = "  ";

          for (i = j = 0; i < nplte; i++, j += 3)
            printf("  %s%3d ->> [%3d, %3d, %3d] = [%02x, %02x, %02x]\n", spc, i,
                   buffer[j], buffer[j + 1], buffer[j + 2],
                   buffer[j], buffer[j + 1], buffer[j + 2]);
        }
      }
      plte_read = 1;
      last_is_idat = 0;
    } else if(strcmp(chunkid, "IDAT") == 0) {
      if (idat_read && !last_is_idat) {
        printf("%s  IDAT chunks must be consecutive\n",verbose?":":fname);
        if (force)
          error = 2;
        else
          return;
      } else if (verbose) {
        printf("\n");
      }

      /* We just want to check that we have read at least the minimum (10)
       * IDAT bytes possible, but avoid any overflow for short ints.  We
       * must also take into account that 0 length IDAT chunks are legal.
       */
      if (idat_read <= 0)
        idat_read = sz > 0 ? sz : -1;
      else if (idat_read < 10)
        idat_read += sz > 10 ? 10 : sz;
      last_is_idat = 1;

		/* Dump the zlib header from the first two bytes. */
		if (zhead < 0x10000 && sz > 0) {
			zhead = (zhead << 8) + buffer[0];
			if (sz > 1 && zhead < 0x10000)
			  zhead = (zhead << 8) + buffer[1];
			if (verbose && zhead >= 0x10000)
			  print_zlibheader(zhead & 0xffff);
		}
    } else if(strcmp(chunkid, "IEND") == 0) {
      if (iend_read) {
        printf("%s  multiple IEND not allowed\n",verbose?":":fname);
        error = 1;
      } else if (sz != 0) {
        printf("%s  incorrect %slength\n",
               verbose?":":fname, verbose?"":"IEND ");
        error = 1;
      } else if (idat_read < 10) {
        printf("%s  not enough IDAT data\n", verbose ? ":":fname);
      } else if (verbose) {
        printf("\n");
      }

      iend_read = 1;
      last_is_idat = 0;
    } else if(strcmp(chunkid, "bKGD") == 0) {
      if (have_bkgd) {
        printf("%s  multiple bKGD not allowed\n",verbose?":":fname);
        error = 1;
      } else if (idat_read) {
        printf("%s  %smust precede IDAT\n",
               verbose?":":fname, verbose?"":"bKGD ");
        error = 1;
      }
      switch (ityp) {
        case 0:
        case 4:
          if (sz != 2) {
            printf("%s  incorrect %slength\n",
                   verbose?":":fname, verbose?"":"bKGD ");
            error = 2;
          }
          if (verbose && (error == 0 || (error == 1 && force))) {
            printf(": gray = %d\n", SH(buffer));
          }
          break;
        case 2:
        case 6:
          if (sz != 6) {
            printf("%s  incorrect %slength\n",
                   verbose?":":fname, verbose?"":"bKGD ");
            error = 2;
          }
          if (verbose && (error == 0 || (error == 1 && force))) {
            printf(": red = %d green = %d blue = %d\n", SH(buffer),
                SH(buffer+2), SH(buffer+4));
          }
          break;
        case 3:
          if (sz != 1) {
            printf("%s  incorrect %slength\n",
                   verbose?":":fname, verbose?"":"bKGD ");
            error = 2;
          }
          if (verbose && (error == 0 || (error == 1 && force))) {
            printf(": index = %d\n", buffer[0]);
          }
          break;
      }
      have_bkgd = 1;
      last_is_idat = 0;
    } else if(strcmp(chunkid, "cHRM") == 0) {
      if (have_chrm) {
        printf("%s  multiple cHRM not allowed\n",verbose?":":fname);
        error = 1;
      } else if (plte_read) {
        printf("%s  %smust precede PLTE\n",
               verbose?":":fname, verbose?"":"cHRM ");
        error = 1;
      } else if (idat_read) {
        printf("%s  %smust precede IDAT\n",
               verbose?":":fname, verbose?"":"cHRM ");
        error = 1;
      } else if (sz != 32) {
        printf("%s  incorrect %slength\n",
               verbose?":":fname, verbose?"":"cHRM ");
        error = 2;
      }
      if (error == 0 || (error == 1 && force)) {
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
                 verbose?":":fname, verbose?"":"cHRM ", wx, wy);
          error = 1;
        } else if (rx < 0 || rx > 0.8 || ry < 0 || ry > 0.8 || rx + ry > 1.0) {
          printf("%s  invalid %sred point %0g %0g\n",
                 verbose?":":fname, verbose?"":"cHRM ", rx, ry);
          error = 1;
        } else if (gx < 0 || gx > 0.8 || gy < 0 || gy > 0.8 || gx + gy > 1.0) {
          printf("%s  invalid %sgreen point %0g %0g\n",
                 verbose?":":fname, verbose?"":"cHRM ", gx, gy);
          error = 1;
        } else if (bx < 0 || bx > 0.8 || by < 0 || by > 0.8 || bx + by > 1.0) {
          printf("%s  invalid %sblue point %0g %0g\n",
                 verbose?":":fname, verbose?"":"cHRM ", bx, by);
          error = 1;
        }
        else if (verbose) {
          printf("\n");
        }

        if (verbose && (error == 0 || (error == 1 && force))) {
          printf("    White x = %0g y = %0g,  Red x = %0g y = %0g\n",
                 wx, wy, rx, ry);
          printf("    Green x = %0g y = %0g,  Blue x = %0g y = %0g\n",
                 gx, gy, bx, by);
        }
      }
      have_chrm = 1;
      last_is_idat = 0;
    } else if(strcmp(chunkid, "gAMA") == 0) {
      if (have_gama) {
        printf("%s  multiple gAMA not allowed\n",verbose?":":fname);
        error = 1;
      } else if (idat_read) {
        printf("%s  %smust precede IDAT\n",
               verbose?":":fname, verbose?"":"gAMA ");
        error = 1;
      } else if (plte_read) {
        printf("%s  %smust precede PLTE\n",
               verbose?":":fname, verbose?"":"gAMA ");
        error = 1;
      } else if (sz != 4) {
        printf("%s  incorrect %slength\n",
                     verbose?":":fname, verbose?"":"gAMA ");
        error = 2;
      }
      if (verbose && (error == 0 || (error == 1 && force))) {
        printf(": %#0.5g\n", (double)LG(buffer)/100000);
      }
      have_gama = 1;
      last_is_idat = 0;
    } else if(strcmp(chunkid, "hIST") == 0) {
      if (have_hist) {
        printf("%s  multiple hIST not allowed\n",verbose?":":fname);
        error = 1;
      } else if (!plte_read) {
        printf("%s  %smust follow PLTE\n",
               verbose?":":fname, verbose?"":"hIST ");
        error=1;
      } else if (idat_read) {
        printf("%s  %smust precede IDAT\n",
               verbose?":":fname, verbose?"":"hIST ");
        error = 1;
      } else if (sz != nplte * 2) {
        printf("%s  invalid number of %sentries (%g)\n",
               verbose?":":fname, verbose?"":"hIST ", (double)sz / 2);
        error = 2;
      }
      if (verbose && (error == 0 || (error == 1 && force))) {
        printf(": %ld histogram entries\n", sz / 2);
      }
      have_hist = 1;
      last_is_idat = 0;
    } else if(strcmp(chunkid, "oFFs") == 0) {
      if (have_offs) {
        printf("%s  multiple oFFs not allowed\n",verbose?":":fname);
        error = 1;
      } else if (idat_read) {
        printf("%s  %smust precede IDAT\n",
               verbose?":":fname, verbose?"":"oFFs ");
        error = 1;
      } else if (sz != 9) {
        printf("%s  incorrect %slength\n",
               verbose?":":fname, verbose?"":"oFFs ");
        error = 2;
      }
      if (verbose && (error == 0 || (error == 1 && force))) {
        printf(": %ldx%ld %s offset\n", LG(buffer), LG(buffer +4),
               buffer[8]==0?"pixels":buffer[8]==1?"micrometers":"unknown unit");
      }
      have_offs = 1;
      last_is_idat = 0;
    } else if(strcmp(chunkid, "pHYs") == 0) {
      if (have_phys) {
        printf("%s  multiple pHYS not allowed\n",verbose?":":fname);
        error = 1;
      } else if (idat_read) {
        printf("%s  %smust precede IDAT\n",
               verbose?":":fname, verbose?"":"pHYS ");
        error = 1;
      } else if (sz != 9) {
        printf("%s  incorrect %slength\n",
               verbose?":":fname, verbose?"":"pHYS ");
        error = 2;
      }
      if (verbose && (error == 0 || (error == 1 && force))) {
        printf(": %ldx%ld pixels/%s\n", LG(buffer), LG(buffer +4),
               buffer[8]==0?"unit":buffer[0]==1?"meter":"unknown unit");
      }
      have_phys = 1;
      last_is_idat = 0;
    } else if(strcmp(chunkid, "sBIT") == 0) {
      int maxbits;

      maxbits = ityp == 3 ? 8 : bits;

      if (have_sbit) {
        printf("%s  multiple sBIT not allowed\n",verbose?":":fname);
        error = 1;
      } else if (plte_read) {
        printf("%s  %smust precede PLTE\n",
               verbose?":":fname, verbose?"":"sBIT ");
        error = 1;
      } else if (idat_read) {
        printf("%s  %smust precede IDAT\n",
               verbose?":":fname, verbose?"":"sBIT ");
        error = 1;
      }
      switch (ityp) {
        case 0:
          if (sz != 1) {
            printf("%s  incorrect %slength\n",
                   verbose?":":fname, verbose?"":"sBIT ");
            error = 2;
          } else if (buffer[0] == 0 || buffer[0] > maxbits) {
            printf("%s  %d %sgrey bits not valid for %dbit/sample image\n",
                   verbose?":":fname, buffer[0], verbose?"":"sBIT ", maxbits);
            error = 1;
          } else if (verbose && (error == 0 || (error == 1 && force))) {
              printf(": gray = %d\n", buffer[0]);
          }
          break;
        case 2:
        case 3:
          if (sz != 3) {
            printf("%s  incorrect %slength\n",
                   verbose?":":fname, verbose?"":"sBIT ");
            error = 2;
          } else if (buffer[0] == 0 || buffer[0] > maxbits) {
            printf("%s  %d %sred bits not valid for %dbit/sample image\n",
                   verbose?":":fname, buffer[0], verbose?"":"sBIT ", maxbits);
            error = 1;
          } else if (buffer[1] == 0 || buffer[1] > maxbits) {
            printf("%s  %d %sgreen bits not valid for %dbit/sample image\n",
                   verbose?":":fname, buffer[1], verbose?"":"sBIT ", maxbits);
            error = 1;
          } else if (buffer[2] == 0 || buffer[2] > maxbits) {
            printf("%s  %d %sblue bits not valid for %dbit/sample image\n",
                   verbose?":":fname, buffer[2], verbose?"":"sBIT ", maxbits);
            error = 1;
          } else if (verbose && (error == 0 || (error == 1 && force))) {
            printf(": red = %d green = %d blue = %d\n",
                   buffer[0], buffer[1], buffer[2]);
          }
          break;
        case 4:
          if (sz != 2) {
            printf("%s  incorrect %slength\n",
                   verbose?":":fname, verbose?"":"sBIT ");
            error = 2;
          } else if (buffer[0] == 0 || buffer[0] > maxbits) {
            printf("%s  %d %sgrey bits not valid for %dbit/sample image\n",
                   verbose?":":fname, buffer[0], verbose?"":"sBIT ", maxbits);
            error = 1;
          } else if (buffer[1] == 0 || buffer[1] > maxbits) {
            printf("%s  %d %salpha bits(tm) not valid for %dbit/sample image\n",
                   verbose?":":fname, buffer[1], verbose?"":"sBIT ", maxbits);
            error = 1;
          } else if (verbose && (error == 0 || (error == 1 && force))) {
            printf(": gray = %d alpha = %d\n", buffer[0], buffer[1]);
          }
          break;
        case 6:
          if (sz != 4) {
            printf("%s  incorrect %slength\n",
                   verbose?":":fname, verbose?"":"sBIT ");
            error = 2;
          } else if (buffer[0] == 0 || buffer[0] > maxbits) {
            printf("%s  %d %sred bits not valid for %dbit/sample image\n",
                   verbose?":":fname, buffer[0], verbose?"":"sBIT ", maxbits);
            error = 1;
          } else if (buffer[1] == 0 || buffer[1] > maxbits) {
            printf("%s  %d %sgreen bits not valid for %dbit/sample image\n",
                   verbose?":":fname, buffer[1], verbose?"":"sBIT ", maxbits);
            error = 1;
          } else if (buffer[2] == 0 || buffer[2] > maxbits) {
            printf("%s  %d %sblue bits not valid for %dbit/sample image\n",
                   verbose?":":fname, buffer[2], verbose?"":"sBIT ", maxbits);
            error = 1;
          } else if (buffer[3] == 0 || buffer[3] > maxbits) {
            printf("%s  %d %salpha bits(tm) not valid for %dbit/sample image\n",
                   verbose?":":fname, buffer[3], verbose?"":"sBIT ", maxbits);
            error = 1;
          } else if (verbose && (error == 0 || (error == 1 && force))) {
            printf(": red = %d green = %d blue = %d alpha = %d\n",
                   buffer[0], buffer[1], buffer[2], buffer[3]);
          }
          break;
      }
      have_sbit = 1;
      last_is_idat = 0;
    } else if(strcmp(chunkid, "sCAL") == 0) {
      if (have_scal) {
        printf("%s  multiple sCAL not allowed\n",verbose?":":fname);
        error = 1;
      } else if (idat_read) {
        printf("%s  %smust precede IDAT\n",
               verbose?":":fname, verbose?"":"sCAL ");
        error = 1;
      }
      if (verbose && (error == 0 || force)) {
        buffer[sz] = '\0';
        buffer[strlen(buffer+1)+1] = 'x';

        printf(": image size %s %s\n", buffer+1,
               buffer[0] == 1?"meters":buffer[0] == 2?"radians":"unknown unit");
      }
      have_scal = 1;
      last_is_idat = 0;
    } else if(strcmp(chunkid, "tEXt") == 0 || strcmp(chunkid, "zTXt") == 0) {
      if(keywordlen(buffer, toread)==0) {
        printf("%s  zero length %s keyword\n",
               verbose?":":fname, verbose?"":chunkid);
        error = 1;
      } else if(keywordlen(buffer, toread)>=80) {
        printf("%s  %s keyword is more than 80 characters\n",
               verbose?":":fname, verbose?"":chunkid);
        error = 1;
      } else {
        int i;

        for(i = 0;i < keywordlen(buffer, toread); i++) {
          if((unsigned char)buffer[i]<32 || ((unsigned char)buffer[i]>=127 &&
             (unsigned char)buffer[i]<160)) {
            printf("%s  %s keyword contains control characters\n",
                   verbose? ":":fname, chunkid);
            error = 1;
            break;
          }
        }

      }
      if (error == 0 || force) {
        init_printbuffer(fname);

        if (printtext && strcmp(chunkid, "tEXt") == 0) {
          if (verbose) printf("\n");

          if(strlen((char *)buffer) < toread)
            buffer[strlen((char *)buffer)]=':';

          printbuffer(buffer, toread, 1);

          while (sz > toread) {
            sz -= toread;
            toread = (sz > BS)? BS : sz;

            if(fread(buffer, 1, toread, fp) != toread) {
              printf("\n  EOF while reading chunk data (tEXT)\n");
              return;
            }
				if (fpOut != NULL) (void)fwrite(buffer, 1, toread, fpOut);

            crc = update_crc(crc, (unsigned char *)buffer, toread);

            printbuffer(buffer, toread, 1);
          }
          printf("\n");
        } else if (verbose) {
          printf(", keyword: ");
          printbuffer(buffer, keywordlen(buffer, toread), 1);
          printf("\n");
        } else if (printtext) {
          printf("zTXT keyword: ");
          printbuffer(buffer, keywordlen(buffer, toread), 1);
          printf("\n");
        }

        finish_printbuffer(fname);
      }
      last_is_idat = 0;
    } else if(strcmp(chunkid, "tIME") == 0) {
      if (have_time) {
        printf("%s  multiple tIME not allowed\n", verbose?":":fname);
        error=1;
      } else if (sz != 7) {
        printf("%s  incorrect %slength\n",
                   verbose?":":fname, verbose?"":"tIME ");
        error = 2;
      }
      /* Print the date in RFC 1123 format, rather than stored order */
      if (verbose && (error == 0 || (error == 1 && force))) {
        printf(": %2d %s %4d %02d:%02d:%02d GMT\n", buffer[3],
               getmonth(buffer[2]), SH(buffer), buffer[4], buffer[5], buffer[6]);
      }
      have_time = 1;
      last_is_idat = 0;
    } else if(strcmp(chunkid, "tRNS") == 0) {
      if (have_trns) {
        printf("%s  multiple tRNS not allowed\n", verbose?":":fname);
        error = 1;
      } else if (ityp == 3 && !plte_read) {
        printf("%s  %smust follow PLTE\n",
               verbose?":":fname, verbose?"":"tRNS ");
        error = 1;
      } else if (idat_read) {
        printf("%s  %smust precede IDAT\n",
               verbose?":":fname, verbose?"":"tRNS ");
        error = 1;
      }
      switch (ityp) {
        case 0:
          if (sz != 2) {
            printf("%s  incorrect %slength for %s image\n",
                   verbose?":":fname, verbose?"":"tRNS ",type[ityp]);
            error = 2;
          }
          else if (verbose && (error == 0 || (error == 1 && force))) {
            printf(": gray = %d\n", SH(buffer));
          }
          break;
        case 2:
          if (sz != 6) {
            printf("%s  incorrect %slength for %s image\n",
                   verbose?":":fname, verbose?"":"tRNS ",type[ityp]);
            error = 2;
          }
          else if (verbose && (error == 0 || (error == 1 && force))) {
            printf(": red = %d green = %d blue = %d\n", SH(buffer),
                   SH(buffer+2), SH(buffer+4));
          }
          break;
        case 3:
          if (sz > nplte) {
            printf("%s  incorrect %slength for %s image\n",
                   verbose?":":fname, verbose?"":"tRNS ", type[ityp]);
            error = 2;
          }
          else if (verbose && (error == 0 || (error == 1 && force))) {
            printf(": %ld transparency entries\n", sz);
          }
          break;
        default:
          printf("%s  %snot allowed in %s image\n",
                 verbose?":":fname, verbose?"":"tRNS ", type[ityp]);
          error = 1;
          break;
      }
      have_trns = 1;
      last_is_idat = 0;
    } else {

      /* A critical safe-to-copy chunk is an error */
      if (!(chunkid[0] & 0x20) && chunkid[3] & 0x20) {
        printf("%s  illegal critical, safe-to-copy chunk%s%s\n",
               verbose?":":fname, verbose?"":" ", verbose?"":chunkid);
        error = 1;
      }
      else if (verbose) {
        printf(": unknown %s%s%s%s chunk\n",
               chunkid[0] & 0x20 ? "ancillary " : "critical ",
               chunkid[1] & 0x20 ? "private " : "",
               chunkid[2] & 0x20 ? "reserved-bit-set " : "",
               chunkid[3] & 0x20 ? "safe-to-copy" : "unsafe-to-copy");
      }

      last_is_idat = 0;
    }

    while (sz > toread) {
      sz -= toread;
      toread = (sz > BS)? BS : sz;

      if(fread(buffer, 1, toread, fp) != toread) {
        printf("%s  EOF while reading chunk data (%s)\n",
               verbose ? "":fname, chunkid);
        return;
      }
		if (fpOut != NULL) (void)fwrite(buffer, 1, toread, fpOut);

      crc = update_crc(crc, (unsigned char *)buffer, toread);
    }

    filecrc = getlong(fp, fname);
	 if (fpOut != NULL) putlong(fpOut, filecrc);

    if(filecrc != CRCCOMPL(crc)) {
      printf("%s  CRC error in chunk %s (actual %08lx, should be %08lx)\n",
             verbose? "":fname, chunkid, CRCCOMPL(crc), filecrc);
        error = 1;
    }

    if (!force && error)
      return;

	 if (iend_read && searching) /* Read the iend, start looking again. */
	   return;
  }

  if(!iend_read) {
    printf("%s  file doesn't end with an IEND chunk\n", verbose? "":fname);
    return;
  }

  if(!error) {
    if(verbose) { /* already printed IHDR info */
      printf("No errors detected in %s.\n", fname);
    } else if (!quiet) {
      printf("No errors detected in %s (%ldx%ld, %d-bit %s, %sinterlaced).\n",
             fname, w, h, bits, (ityp > 6)? type[1]:type[ityp],
             lace? "":"non-");
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
	/* Use this if file name length is restricted". */
	sprintf(name, "PNG%d", ipng);
	szdot = name;
#endif

	if (extracting) {
	  static char magic[8] = {(char)137, (char)80, 78, 71, 13, 10, 26, 10};

	  szdot += strlen(szdot);
	  strcpy(szdot, ".png");
	  fpOut = fopen(name, "wb");
	  if (fpOut == NULL) {
		  perror(name);
		  fprintf(stderr, "%s: could not write output (ignored)\n", name);
	  } else
		  fprintf(stderr, "%s: contains %s PNG %d\n", name, fname, ipng);
	  (void)fwrite(magic, 8, 1, fpOut);
	  *szdot = 0;
	}

	pngcheck(fp, name, 1, fpOut);
	if (fpOut != NULL) {
	  int err;
	  (void)fflush(fpOut);
	  err = ferror(fpOut);
	  if (fclose(fpOut)) err = 1;
	  if (err) {
		  perror(name); /* Probably invalid by now. */
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
	 fprintf(stderr, "Extracting PNGs from %s\n", fname);

  /* This works because the leading 137 code is not repeated in
     the magic, so partial matches may be discarded if we fail a match. */
  do {
	  ch = getc(fp);
	  while (ch == 137) {
	    if ((ch = getc(fp)) == 80 &&
		     (ch = getc(fp)) == 78 &&
		     (ch = getc(fp)) == 71 &&
		     (ch = getc(fp)) == 13 &&
		     (ch = getc(fp)) == 10 &&
		     (ch = getc(fp)) == 26 &&
		     (ch = getc(fp)) == 10) {
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
        verbose=1;
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
      default:
        fprintf(stderr, "unknown option %c\n", argv[1][i]);
        goto usage;
    }
  }

  if(argc == 1) {
    if (isatty(0)) {       /* if stdin not redirected, give the user help */
usage:
      fprintf(stderr, "PNGcheck, version %s\n", VERSION);
      fprintf(stderr, "   by Alexander Lehmann and Andreas Dilger.\n");
      fprintf(stderr, "Test a PNG image file for corruption.\n\n");
      fprintf(stderr, "Usage:  pngcheck [-vqt7f] file.png [file.png [...]]\n");
      fprintf(stderr, "   or:  ... | pngcheck [-vqt7f]\n\n");
		fprintf(stderr, "   or:  pngcheck -{sx}[vqt7f] file-containing-pngs...\n");
      fprintf(stderr, "Options:\n");
      fprintf(stderr, "   -v  test verbosely (output most chunk data)\n");
      fprintf(stderr, "   -q  test quietly (only output errors)\n");
      fprintf(stderr, "   -t  print contents of tEXt chunks (can be used with -q)\n");
      fprintf(stderr, "   -7  print contents of tEXt chunks, escape chars >=128 (for 7bit terminals)\n");
      fprintf(stderr, "   -p  print contents of PLTE chunks (can be used with -q)\n");
      fprintf(stderr, "   -f  force continuation even after major errors\n");
		fprintf(stderr, "   -s  search for PNGs within another file\n");
		fprintf(stderr, "   -x  search for PNGs and extract them when found\n");
    } else if (search) {
		pngsearch(stdin, "stdin", extract);
	 } else {
      pngcheck(stdin, "stdin", 0, NULL);
    }
  } else {
    for(i = 1; i < argc; i++) {
      if ((fp = fopen(argv[i], "rb"))==NULL) {
        perror(argv[i]);
      } else {
		  if (search)
			 pngsearch(fp, argv[i], extract);
		  else
          pngcheck(fp, argv[i], 0, NULL);
        fclose(fp);
      }
    }
  }
  return 0;
}

/*  PNG_subs
 *
 *  Utility routines for PNG encoders and decoders
 *  by Glenn Randers-Pehrson
 *
 */

#define OK      0
#define ERROR   -1
#define WARNING -2

/* (int)PNG_check_magic ((unsigned char*) magic)
 *
 * check the magic numbers in 8-byte buffer at the beginning of
 * a PNG file.
 *
 * by Alexander Lehmann and Glenn Randers-Pehrson
 *
 * This is free software; you can redistribute it and/or modify it
 * without any restrictions.
 *
 */

int PNG_check_magic(unsigned char *magic, char *fname)
{
  if (strncmp((char *)&magic[1], "PNG", 3) != 0) {
    printf("%s  this is not a PNG\n", verbose ? "" : fname);
    return(ERROR);
  }

  if (magic[0] != 0x89 ||
      strncmp((char *)&magic[4], "\015\012\032\012", 4) != 0) {

    if (verbose) {
      printf("  file is CORRUPTED.\n");
    } else {
      printf("%s  file is CORRUPTED by text conversion.\n", fname);
      return (ERROR);
    }

    /* This coding derived from Alexander Lehmanns checkpng code   */
    if(strncmp((char *)&magic[4], "\012\032", 2) == 0)
      printf("  It seems to have suffered DOS->unix conversion\n");

    else if(strncmp((char *)&magic[4], "\015\032", 2) == 0)
      printf("  It seems to have suffered DOS->Mac conversion\n");

    else if(strncmp((char *)&magic[4], "\015\015\032", 3) == 0)
      printf("  It seems to have suffered unix->Mac conversion\n");

    else if(strncmp((char *)&magic[4], "\012\012\032", 3) == 0)
      printf("  It seems to have suffered Mac->unix conversion\n");

    else if(strncmp((char *)&magic[4], "\012\012", 2) == 0)
      printf("  It seems to have suffered DOS->unix conversion\n");

    else if(strncmp((char *)&magic[4], "\015\015\012\032", 4) == 0)
      printf("  It seems to have suffered unix->DOS conversion\n");

    else if(strncmp((char *)&magic[4], "\015\012\032\015", 4) == 0)
      printf("  It seems to have suffered unix->DOS conversion\n");

    else if(strncmp((char *)&magic[4], "\015\012\012", 3) == 0)
      printf("  It seems to have suffered DOS EOF conversion\n");

    else if(strncmp((char *)&magic[4], "\015\012\032\012", 4) != 0)
      printf("  It seems to have suffered EOL conversion\n");

    if (magic[0] == 9)
      printf("  It was probably transmitted through a 7bit channel\n");
    else if(magic[0] != 0x89)
      printf("  It was probably transmitted in text mode\n");

    return(ERROR);
  }

  return (OK);
}

/* (int)PNG_check_magic ((char*) magic)
 *
 * from Alex Lehmann
 *
 */

int PNG_check_chunk_name(char *chunk_name, char *fname)
{
  if(!isalpha(chunk_name[0]) || !isalpha(chunk_name[1]) ||
     !isalpha(chunk_name[2]) || !isalpha(chunk_name[3])) {
    printf("%s%schunk name %02x %02x %02x %02x doesn't comply to naming rules\n",
           verbose ? "" : fname, verbose ? "" : ": ",
           chunk_name[0], chunk_name[1], chunk_name[2], chunk_name[3]);
    return (ERROR);
  }
  else return (OK);
}
