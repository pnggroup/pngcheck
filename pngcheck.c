/*
 * authenticate a PNG file (as per draft 10)
 *
 * this program checks the PNG identifier with conversion checks,
 * the file structure and the chunk CRCs.
 *
 *
 * written by Alexander Lehmann <alex@hal.rhein-main.de>
 *
 *
 * 95.02.23 fixed wrong magic numbers
 *
 * 95.03.13 crc code from png spec, compiles on memory impaired PCs now,
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
 * 95.06.01 :-) GRR: reformatted; print tEXt and zTXt keywords; add usage
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
 */

#define VERSION "1.92 of 5 January 1996"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

int PNG_check_magic(unsigned char *magic);
int PNG_check_chunk_name(char *chunk_name);
void make_crc_table(void);
unsigned long update_crc(unsigned long crc, unsigned char *buf, int len);
unsigned long getlong(FILE *fp);
void init_printbuffer(void);
void printbuffer(char *buffer, int size, int printtext);
void finish_printbuffer(void);
int keywordlen(char *buffer, int maxsize);
char *getmonth(int m);
void pngcheck(FILE *fp, char *_fname);

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
int sevenbit; /* escape characters >=160 */
int error; /* an error already occured, but we are continueing */
int force; /* continue even if a really bad error occurs (CRC error, etc) */
char *fname;
unsigned char buffer[BS];

/* table of crc's of all 8-bit messages */
unsigned long crc_table[256];

/* Flag: has the table been computed? Initially false. */
int crc_table_computed = 0;

/* make the table for a fast crc */
void make_crc_table(void)
{
  unsigned long c;
  int n, k;

  for (n = 0; n < 256; n++) {
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

unsigned long getlong(FILE *fp)
{
  unsigned long res=0;
  int c;
  int i;

  for(i=0;i<4;i++) {
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

/* print out size characters in buffer, take care to not print control chars
   other than whitespace, since this may open ways of attack by so-called
   ANSI-bombs */

int cr;
int lf;
int nul;
int control;
int esc;

void init_printbuffer(void)
{
  cr=0;
  lf=0;
  nul=0;
  control=0;
  esc=0;
}

void printbuffer(char *buffer, int size, int printtext)
{
  unsigned char c;

  while(size--) {
    c = *buffer++;
    if(printtext)
      if((c < ' ' && c != '\t' && c != '\n') ||
         (sevenbit ? c > 127 : (c >= 127 && c < 160)))
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

void finish_printbuffer(void)
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
  {"(undefined)", "January", "February", "March", "April", "May", "June",
   "July", "August", "September", "October", "November", "December"};

  if(m<1 || m>12) return month[0];
  else return month[m];
}

void pngcheck(FILE *fp, char *_fname)
{
  long s;
  unsigned char magic[8];
  char chunkid[5];
  int toread;
  int c;
  unsigned long crc, filecrc;
  int ihdr_read=0;
  int plte_read=0;
  int idat_read=0;
  int last_is_idat=0;
  int iend_read=0;
  int have_bkgd = 0, have_chrm = 0, have_gama = 0, have_hist = 0, have_offs = 0;
  int have_phys = 0, have_sbit = 0, have_scal = 0, have_time = 0, have_trns = 0;
  long w = 0, h = 0;
  int bits = 0, ityp = 0, lace = 0, nplte = 0;
  static int first_file=1;
  int i;
  static char *type[] = {"grayscale", "undefined type", "RGB", "colormap",
                           "grayscale+alpha", "undefined type", "RGB+alpha"};

  fname=_fname; /* make filename available to functions above */

  error = 0;

  if(verbose || printtext) {
    printf("%sFile: %s\n", first_file?"\n":"", fname);
  }

  first_file = 0;

  if(fread(magic, 1, 8, fp)!=8) {
    printf("%s  cannot read PNG header\n", verbose? "":fname);
    return;
  }

  if (PNG_check_magic(magic) != 0) {
    /* maybe it's a MacBinary file */

    if(magic[0]==0 && magic[1]>0 && magic[1]<=64 && magic[2]!=0) {
      if (!quiet)
        printf("%s  (trying to skip MacBinary header)\n", verbose? "":fname);
      if(fread(buffer, 1, 120, fp)==120 &&
         fread(magic, 1, 8, fp)==8 &&
         PNG_check_magic(magic) == 0) {
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

    s=getlong(fp);

    if(fread(chunkid, 1, 4, fp)!=4) {
      printf("%s  EOF while reading chunk type\n", verbose? "":fname);
      return;
    }

    chunkid[4] = 0;

    if (PNG_check_chunk_name(chunkid) != 0) {
      if (force)
        error = 2;
      else
        return;
    }

    if(verbose)
      printf("  chunk %s at offset 0x%05lx, length %ld", chunkid,
             ftell(fp)-4, s);

    crc = update_crc(CRCINIT, (unsigned char *)chunkid, 4);

    if(!ihdr_read && strcmp(chunkid,"IHDR")!=0) {
        printf("\n%s  file doesn't start with a IHDR chunk\n",
               verbose? "":fname);
	if (force)
          error = 2;
	else
	  return;
    }

    toread = (s>BS)? BS : s;

    if(fread(buffer, 1, toread, fp)!=toread) {
      printf("\n%s  EOF while reading chunk data (%s)\n",
             verbose ? "":fname, chunkid);
      return;
    }

    crc = update_crc(crc, (unsigned char *)buffer, toread);

    if(strcmp(chunkid, "IHDR") == 0) {
      if (ihdr_read) {
        printf("%s  multiple IHDR not allowed\n", verbose?":":fname);
        error = 1;
      } else if (s != 13) {
        printf("%s  incorrect %slength\n",
               verbose?":":fname, verbose?"":"IHDR ");
        error = 2;
      }
      if (error == 0 || (error == 1 && force)) {
        w = LG(buffer);
        h = LG(buffer+4);
        bits = (unsigned)buffer[8];
        ityp = (unsigned)buffer[9];
        switch (bits) {
          case 1:
          case 2:
          case 4:
            if (ityp == 2 || ityp == 4 || ityp == 6) {/* RGB or GA or RGBA */
              printf("%s  invalid %sbit depth (%d) for %s image\n",
                     verbose?":":fname, verbose?"":"IHDR ",bits, type[ityp]);
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
      } else if (s < 3 || s > 768 || s % 3 != 0) {
        printf("%s  invalid number of %sentries (%g)\n",
               verbose?":":fname, verbose?"":"PLTE ",(double)s / 3);
        error = 2;
      }
      if (error == 0 || (error == 1 && force)) {
        nplte = s / 3;
        if (verbose)
          printf(": %d palette entries\n", nplte);
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

      idat_read = 1;
      last_is_idat = 1;
    } else if(strcmp(chunkid, "IEND") == 0) {
      if (iend_read) {
        printf("%s  multiple IEND not allowed\n",verbose?":":fname);
        error = 1;
      } else if (s != 0) {
        printf("%s  incorrect %slength\n",
               verbose?":":fname, verbose?"":"IEND ");
        error = 1;
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
          if (s != 2) {
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
          if (s != 6) {
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
          if (s != 1) {
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
      } else if (s != 32) {
        printf("%s  incorrect %slength\n",
               verbose?":":fname, verbose?"":"cHRM ");
        error = 2;
      }
      if (error == 0 || (error == 1 && force))
      {
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
          printf("%s  Invalid %swhite point %0g %0g\n",
                 verbose?":":fname, verbose?"":"cHRM ", wx, wy);
          error = 1;
        } else if (rx < 0 || rx > 0.8 || ry < 0 || ry > 0.8 || rx + ry > 1.0) {
          printf("%s  Invalid %sred point %0g %0g\n",
                 verbose?":":fname, verbose?"":"cHRM ", rx, ry);
          error = 1;
        } else if (gx < 0 || gx > 0.8 || gy < 0 || gy > 0.8 || gx + gy > 1.0) {
          printf("%s  Invalid %sgreen point %0g %0g\n",
                 verbose?":":fname, verbose?"":"cHRM ", gx, gy);
          error = 1;
        } else if (bx < 0 || bx > 0.8 || by < 0 || by > 0.8 || bx + by > 1.0) {
          printf("%s  Invalid %sblue point %0g %0g\n",
                 verbose?":":fname, verbose?"":"cHRM ", bx, by);
          error = 1;
        } 
        else
        {
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
      } else if (s != 4) {
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
      } else if (s != nplte * 2) {
        printf("%s  invalid number of %sentries (%g)\n",
               verbose?":":fname, verbose?"":"hIST ", (double)s / 2);
        error = 2;
      }
      if (verbose && (error == 0 || (error == 1 && force))) {
        printf(": %ld histogram entries\n", s / 2);
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
      } else if (s != 9) {
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
      } else if (s != 9) {
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
          if (s != 1) {
            printf("%s  incorrect %slength\n",
                   verbose?":":fname, verbose?"":"sBIT ");
            error = 2;
          } else if (verbose && (error == 0 || (error == 1 && force))) {
              printf(": gray = %d\n", buffer[0]);
          }
          break;
        case 2:
        case 3:
          if (s != 3) {
            printf("%s  incorrect %slength\n",
                   verbose?":":fname, verbose?"":"sBIT ");
            error = 2;
          } else if (verbose && (error == 0 || (error == 1 && force))) {
            printf(": red = %d green = %d blue = %d\n",
                   buffer[0], buffer[1], buffer[2]);
          }
          break;
        case 4:
          if (s != 2) {
            printf("%s  incorrect %slength\n",
                   verbose?":":fname, verbose?"":"sBIT ");
            error = 2;
          } else if (verbose && (error == 0 || (error == 1 && force))) {
            printf(": gray = %d alpha = %d\n", buffer[0], buffer[1]);
          }
          break;
        case 6:
          if (s != 4) {
            printf("%s  incorrect %slength\n",
                   verbose?":":fname, verbose?"":"sBIT ");
            error = 2;
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
        buffer[s] = '\0';
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
        for(i=0;i<keywordlen(buffer, toread);i++) {
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
        init_printbuffer();

        if (printtext && strcmp(chunkid, "tEXt") == 0) {
          if (verbose) printf("\n");

          if(strlen((char *)buffer)<toread)
            buffer[strlen((char *)buffer)]=':';

          printbuffer(buffer, toread, 1);

          while (s > toread) {
            s -= toread;
            toread = (s>BS)? BS : s;

            if(fread(buffer, 1, toread, fp)!=toread) {
              printf("\n  EOF while reading chunk data (tEXT)\n");
              return;
            }

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

        finish_printbuffer();
      }
      last_is_idat = 0;
    } else if(strcmp(chunkid, "tIME") == 0) {
      if (have_time) {
        printf(": multiple tIME not allowed\n");
        error=1;
      } else if (s != 7) {
        printf("%s  incorrect %slength\n",
                   verbose?":":fname, verbose?"":"tIME ");
        error = 2;
      }
      if (verbose && (error == 0 || (error == 1 && force))) {
        printf(": %d %s %d %02d:%02d:%02d GMT\n", SH(buffer),
               getmonth(buffer[2]), buffer[3], buffer[4], buffer[5], buffer[6]);
      }
      have_time = 1;
      last_is_idat = 0;
    } else if(strcmp(chunkid, "tRNS") == 0) {
      if (have_trns) {
        printf("%s  multiple tRNS not allowed\n", verbose?":":fname);
        error = 1;
      } else if (!plte_read) {
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
          if (s != 2)
          {
            printf("%s  incorrect %slength for %s image\n",
                   verbose?":":fname, verbose?"":"tRNS ",type[ityp]);
            error = 2;
          }
          else if (verbose && (error == 0 || (error == 1 && force))) {
            printf(": gray = %d\n", SH(buffer));
          }
        case 2:
          if (s != 6)
          {
            printf("%s  incorrect %slength for %s image\n",
                   verbose?":":fname, verbose?"":"tRNS ",type[ityp]);
            error = 2;
          }
          else if (verbose && (error == 0 || (error == 1 && force))) {
            printf(": red = %d green = %d blue = %d\n", SH(buffer),
                   SH(buffer+2), SH(buffer+4));
          }
        case 3:
          if (s > nplte)
          {
            printf("%s  incorrect %slength for %s image\n",
                   verbose?":":fname, verbose?"":"tRNS ",type[ityp]);
            error = 2;
          }
          else if (verbose && (error == 0 || (error == 1 && force))) {
            printf(": %ld transparency entries\n", s);
          }
        default:
          printf("%s  %snot allowed in %s image\n",
                 verbose?":":fname, verbose?"":"tRNS ",type[ityp]);
          error = 1;
      }
      have_trns = 1;
      last_is_idat = 0;
    } else if (verbose) {
      printf(": unknown %s%s%s%s chunk\n",
             chunkid[0]&0x10?"ancillary ":"critical ",
             chunkid[1]&0x10?"private ":"",
             chunkid[2]&0x10?"reserved-bit-set ":"",
             chunkid[3]&0x10?"safe-to-copy":"unsafe-to-copy");

      last_is_idat = 0;
    }

    while (s > toread)
    {
      s -= toread;
      toread = (s>BS)? BS : s;

      if(fread(buffer, 1, toread, fp)!=toread) {
        printf("%s  EOF while reading chunk data (%s)\n",
               verbose ? "":fname, chunkid);
        return;
      }

      crc = update_crc(crc, (unsigned char *)buffer, toread);
    }
    
    filecrc=getlong(fp);

    if(filecrc!=CRCCOMPL(crc)) {
      printf("%s  CRC error in chunk %s (actual %08lx, should be %08lx)\n",
             verbose? "":fname, chunkid, CRCCOMPL(crc), filecrc);
        error = 1;
    }

    if (!force && error)
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

int main(int argc, char *argv[])
{
  FILE *fp;
  int i;

#ifdef __EMX__
  _wildcard(&argc, &argv);   /* Unix-like globbing for OS/2 and DOS */
#endif

  while(argc>1 && argv[1][0]=='-') {
    if(strcmp(argv[1],"-v")==0) {
      verbose=1;
      quiet=0;
      argc--;
      argv++;
    } else
    if(strcmp(argv[1],"-q")==0) {
      verbose=0;
      quiet=1;
      argc--;
      argv++;
    } else
    if(strcmp(argv[1],"-t")==0) {
      printtext=1;
      argc--;
      argv++;
    } else
    if(strcmp(argv[1],"-7")==0) {
      printtext=1;
      sevenbit=1;
      argc--;
      argv++;
    } else
    if(strcmp(argv[1],"-f")==0) {
      force=1;
      argc--;
      argv++;
    } else {
      fprintf(stderr, "unknown option %s\n", argv[1]);
      goto usage;
    }
  }

  if(argc==1) {
    if (isatty(0)) {       /* if stdin not redirected, give the user help */
usage:
      fprintf(stderr, "PNGcheck, version %s, by Alexander Lehmann.\n",
              VERSION);
      fprintf(stderr, "Test a PNG image file for corruption.\n\n");
      fprintf(stderr, "Usage:  pngcheck [-vqt7f] file.png [file.png [...]]\n");
      fprintf(stderr, "   or:  ... | pngcheck [-vqt7f]\n\n");
      fprintf(stderr, "Options:\n");
      fprintf(stderr, "   -v  test verbosely\n");
      fprintf(stderr, "   -q  test quietly (only output errors)\n");
      fprintf(stderr, "   -t  print contents of tEXt chunks (can be used with -q)\n");
      fprintf(stderr, "   -7  print contents of tEXt chunks, escape chars >=128 (for 7bit terminals)\n");
      fprintf(stderr, "   -f  force continuation even after major errors\n");
    } else
      pngcheck(stdin, "stdin");
  } else {
    for(i=1;i<argc;i++) {
      if((fp=fopen(argv[i],"rb"))==NULL) {
        perror(argv[i]);
      } else {
        pngcheck(fp, argv[i]);
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

int PNG_check_magic(unsigned char *magic)
{
  if (strncmp((char *)&magic[1],"PNG",3) != 0) {
             fprintf(stderr, "not a PNG file\n");
             return(ERROR);
            }

    if (magic[0] != 0x89 ||
         strncmp((char *)&magic[4],"\015\012\032\012",4) != 0) {
         fprintf(stderr, "PNG file is CORRUPTED.\n");

         /* this coding taken from Alexander Lehmanns checkpng code   */

        if(strncmp((char *)&magic[4],"\n\032",2) == 0) fprintf
             (stderr," It seems to have suffered DOS->unix conversion\n");
        else
        if(strncmp((char *)&magic[4],"\r\032",2) == 0) fprintf
             (stderr," It seems to have suffered DOS->Mac conversion\n");
        else
        if(strncmp((char *)&magic[4],"\r\r\032",3) == 0) fprintf
             (stderr," It seems to have suffered unix->Mac conversion\n");
        else
        if(strncmp((char *)&magic[4],"\n\n\032",3) == 0) fprintf
             (stderr," It seems to have suffered Mac-unix conversion\n");
        else
        if(strncmp((char *)&magic[4],"\r\n\032\r",4) == 0) fprintf
             (stderr," It seems to have suffered unix->DOS conversion\n");
        else
        if(strncmp((char *)&magic[4],"\r\r\n\032",4) == 0) fprintf
             (stderr," It seems to have suffered unix->DOS conversion\n");
        else
        if(strncmp((char *)&magic[4],"\r\n\032\n",4) != 0) fprintf
             (stderr," It seems to have suffered EOL conversion\n");

        if(magic[0]==9) fprintf
             (stderr," It was probably transmitted through a 7bit channel\n");
        else
        if(magic[0]!=0x89) fprintf
             (stderr,"  It was probably transmitted in text mode\n");
        /*  end of Alexander Lehmann's code  */
        return(ERROR);
        }
    return (OK);
}

/* (int)PNG_check_magic ((char*) magic)
 *
 * from Alex Lehmann
 *
 */

int PNG_check_chunk_name(char *chunk_name)
{
     if(!isalpha(chunk_name[0]) || !isalpha(chunk_name[1]) ||
        !isalpha(chunk_name[2]) || !isalpha(chunk_name[3])) {
         printf("chunk name %02x %02x %02x %02x doesn't comply to naming rules\n",
         chunk_name[0],chunk_name[1],chunk_name[2],chunk_name[3]);
         return (ERROR);
         }
     else return (OK);
}
