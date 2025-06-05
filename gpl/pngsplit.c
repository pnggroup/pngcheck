/* pngsplit.c - split a PNG file into individual chunk-files (and check CRCs)
**
** Downloads:
**
**      http://www.libpng.org/pub/png/apps/pngcheck.html
**
** To compile (assuming zlib path is ../zlib):
**
**      gcc -Wall -O2 -I../zlib pngsplit.c -o pngsplit -L../zlib -lz
**
**
**  Copyright 2005-2020 Greg Roelofs
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**
*/

#define VERSION "1.0 of 31 October 2020"

/*
 * TO DO:
 *  - fix filename-mismatch bookkeeping error (2nd FIXME below)
 *  - convert GET_U32() macro to function (optionally inlinable)
 *  - clean up code, promote to 1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <zlib.h>   // for crc32()

/* to use this, define and setenv MALLOC_TRACE /path/to/pngsplit-mtrace.log */
#undef GRR_MALLOC_DEBUG
#ifdef GRR_MALLOC_DEBUG
#  include <mcheck.h>
#endif

typedef unsigned long  ulg;
typedef unsigned short ush;
typedef unsigned char  uch;

#ifndef TRUE
#  define TRUE     1
#endif
#ifndef FALSE
#  define FALSE    0
#endif

#define FNMAX      1024     /* max filename length */
#define BUFSZ      4096

/* flag bits for writing chunk files */
#define WR_FORCE   0x01
#define WR_OPEN    0x02
#define WR_CLOSE   0x04

#define MIN_PNG_SIZE (8 + 4+4+13+4 + 4+4+10+4 + 4+4+4) // 67 (IDAT = 10: empir.)

#define U16(x)  ( ((ush)(((uch *)(x))[0]) <<  8) |  \
                  ((ush)(((uch *)(x))[1])      ) )

#define U32(x)  ( ((ulg)(((uch *)(x))[0]) << 24) |  \
                  ((ulg)(((uch *)(x))[1]) << 16) |  \
                  ((ulg)(((uch *)(x))[2]) <<  8) |  \
                  ((ulg)(((uch *)(x))[3])      ) )

#define PNGSPLIT_HDR "\
pngsplit, version " VERSION ", by Greg Roelofs.\n\
  This software is licensed under the GNU General Public License.\n\
  There is NO warranty.\n\n"

#define PNGSPLIT_USAGE "\
   usage:  pngsplit [options] pngfile [pngfile [...]]\n\
   options:\n\
      -force         overwrite existing output files\n\
      -verbose       print more status messages (synonym:  -noquiet)\n\n\
   Split a PNG, MNG or JNG file into individual, numbered chunks (filenames\n\
   \"foo.png.0000.sig\", \"foo.png.0001.IHDR\", etc.).\n"

static const uch pngsig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
static const uch mngsig[8] = {138, 77, 78, 71, 13, 10, 26, 10};
static const uch jngsig[8] = {139, 74, 78, 71, 13, 10, 26, 10};

/*
 * ulg IHDR = ( (73L << 24) | (72L << 16) | (68L << 8) | (82L) );  // 0x49484452
 * ulg IDAT = ( (73L << 24) | (68L << 16) | (65L << 8) | (84L) );  // 0x49444154
 * ulg IEND = ( (73L << 24) | (69L << 16) | (78L << 8) | (68L) );  // 0x49454e44
 */


static int pngsplit (char *filename, int force, int verbose);
static char *chunkstr (ulg typ);
static int write_chunk_to_file (char *basename, int baselen, ulg num,
                                ulg chunklen, ulg chunktyp, ulg chunkcrc,
                                uch *buf, ulg len, int flags);



int main(int argc, char *argv[])
{
    char *filename;
    int argn;
    int force = FALSE;
    int verbose = 0;
    int latest_error=0, error_count=0, file_count=0;


#ifdef __EMX__
    _wildcard(&argc, &argv);   /* Unix-like globbing for OS/2 and DOS */
#endif

#ifdef GRR_MALLOC_DEBUG
    mtrace();
#endif

    argn = 1;

    while ( argn < argc && argv[argn][0] == '-' && argv[argn][1] != '\0' ) {
        if ( 0 == strncmp( argv[argn], "-force", 2 ) )
            force = TRUE;
        else if ( 0 == strncmp( argv[argn], "-noforce", 4 ) )
            force = FALSE;
        else if ( 0 == strncmp( argv[argn], "-verbose", 2 ) ||
                  0 == strncmp( argv[argn], "-noquiet", 4 ) )
            ++verbose;
        else if ( 0 == strncmp( argv[argn], "-noverbose", 4 ) ||
                  0 == strncmp( argv[argn], "-quiet", 2 ) )
            verbose = 0;
        else {
            fprintf(stderr, PNGSPLIT_HDR);
            fprintf(stderr, PNGSPLIT_USAGE);
            fflush(stderr);
            return 1;
        }
        ++argn;
    }

    if ( argn == argc ) {
        fprintf(stderr, PNGSPLIT_HDR);
        fprintf(stderr, PNGSPLIT_USAGE);
        fflush(stderr);
        return 5;
    } else {
        filename = argv[argn];
        ++argn;
    }


    /*=============================  MAIN LOOP  =============================*/

    fprintf(stdout, PNGSPLIT_HDR);
    fflush(stdout);

    while (argn <= argc) {
        int retval;

        if (verbose >= 0) {
            printf("%s:\n", filename);
            fflush(stdout);
        }

        retval = pngsplit(filename, force, verbose);

        if (retval) {
            latest_error = retval;
            ++error_count;
        }
        ++file_count;

        if (verbose) {
            printf("\n");
            fflush(stdout);
        }

        filename = argv[argn];
        ++argn;
    }

    /*=======================================================================*/


    if (verbose) {
        if (error_count)
            printf("There were errors splitting %d PNG file%s out of a"
              " total of %d file%s.\n",
              error_count, (error_count == 1)? "" : "s",
              file_count, (file_count == 1)? "" : "s");
        else
            printf("No errors detected while splitting %d PNG image%s.\n",
              file_count, (file_count == 1)? "" : "s");
        fflush(stdout);
    }

    return latest_error;
}





#define GET_U32(val, docrc)						\
    if (incnt < 4) {							\
        int j=incnt, remainder=(4-incnt);				\
        uch tmpbuf[4];							\
        uch *tptr = tmpbuf;						\
									\
        /* copy bytes to temporary buffer */				\
        for (;  j > 0;  --j)						\
            *tptr++ = *inptr++;						\
        /* read more bytes into main buffer */				\
        inptr = inbuf;							\
        incnt = fread(inbuf, 1, BUFSZ, infile);   /* 4096 bytes */	\
        /* if still fewer than 4 bytes, bail */				\
        if (incnt < remainder) {					\
            haveEOF = TRUE;						\
            break;							\
        }								\
        /* copy remaining bytes to temporary buffer... */		\
        for (j = remainder;  j > 0;  --j)				\
            *tptr++ = *inptr++;						\
        incnt -= remainder;						\
        /* ...and assemble into 32-bit int */				\
        val = U32(tmpbuf);						\
        if (docrc)							\
            calc_crc = crc32(calc_crc, tmpbuf, 4);			\
        file_offset += 4;						\
    } else {								\
        val = U32(inptr);						\
        if (docrc)							\
            calc_crc = crc32(calc_crc, inptr, 4);			\
        inptr += 4;							\
        incnt -= 4;							\
        file_offset += 4;						\
    }

    // FIXME:  change GET_U32() macro into a function (too big => cache trash)





static int pngsplit(char *filename, int force, int verbose)
{
    FILE *infile;
    uch inbuf[BUFSZ], *inptr;
    int fnlen, incnt, flags, error, haveEOF;
    ulg chunknum=0L, chunklen, chunktyp, chunkcrc, calc_crc;
    ulg file_offset=0L;


    if (force)
        force = WR_FORCE;  // ensure compatibility with flags below

    if ((infile = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "  error:  cannot open %s for reading\n", filename);
        fflush(stderr);
        return 2;
    }

    fnlen = strlen(filename);

    if (fnlen > FNMAX-12) {
        fprintf(stderr,
          "  warning:  base filename [%s] will be truncated\n", filename);
        fflush(stderr);
        fnlen = FNMAX-12;
    }


    /*
    ** Step 1: read in the input image.
    */

    // check PNG/MNG/JNG header
    inptr = inbuf;
    incnt = fread(inbuf, 1, BUFSZ, infile);   // 4096 bytes

    if (incnt < MIN_PNG_SIZE || (memcmp(inbuf, pngsig, 8) != 0 &&
        memcmp(inbuf, mngsig, 8) != 0 && memcmp(inbuf, jngsig, 8) != 0))
    {
        fprintf(stderr, "  error:  %s does not appear to be a PNG, MNG, or "
          "JNG file\n", filename);
        fflush(stderr);
        return 17;
    }
    inptr += 8;
    incnt -= 8;
    file_offset = 8L;

    flags = force | WR_OPEN | WR_CLOSE;
    if (write_chunk_to_file(filename, fnlen, chunknum, 0,0,0, inbuf, 8, flags))
        return 18;

    error = haveEOF = FALSE;

    while (!error && !haveEOF) {
        // check chunk length, name/ID bytes, and CRC over data
        GET_U32(chunklen, 0)  // this advances inptr, refills buffer as needed
        calc_crc = crc32(0L, Z_NULL, 0);
        GET_U32(chunktyp, 1)  // ...and also updates calc_crc in this case

        ++chunknum;

        error = write_chunk_to_file(filename, fnlen, chunknum, chunklen,
                                    chunktyp, 0, inbuf, 0, WR_OPEN | force);
        if (error)
            return 19;

        // NOTE:  The displayed file offset is that of the chunk name/ID,
        //        *not* the true beginning of the chunk (length-bytes,
        //        4 bytes earlier).  This matches pngcheck's behavior.
        if (verbose) {
            printf("    %s chunk (0x%lx), length %ld, at file offset %lu "
              "(0x%05lx)\n", chunkstr(chunktyp), chunktyp, chunklen,
              file_offset-4, file_offset-4);
        }

        // now pointing at chunk data (i.e., AFTER chunk name/ID)
        while (chunklen > incnt) {
            calc_crc = crc32(calc_crc, inptr, incnt);
            error = write_chunk_to_file(filename, fnlen, chunknum, 0,
                                        0, 0, inptr, incnt, force);
            if (error)
                return 20;

            chunklen -= incnt;
            file_offset += incnt;

            /* read more bytes into buffer */
            inptr = inbuf;
            incnt = fread(inbuf, 1, BUFSZ, infile);   /* 4096 bytes */
            if (incnt <= 0) {
                fprintf(stderr, "  error:  unexpected EOF while reading %s "
                  "(chunk is missing %ld bytes)\n", filename, chunklen);
                fflush(stderr);
                return 21;
            }
        }
        calc_crc = crc32(calc_crc, inptr, chunklen);
        error = write_chunk_to_file(filename, fnlen, chunknum, 0,
                                    0, 0, inptr, chunklen, force);
        if (error)
            return 22;

        inptr += chunklen;
        incnt -= chunklen;
        file_offset += chunklen;

        GET_U32(chunkcrc, 0)
        error = write_chunk_to_file(filename, fnlen, chunknum, 0,
                                    0, chunkcrc, inbuf, 0, WR_CLOSE | force);
        if (error)
            return 23;

        if (calc_crc != chunkcrc) {
            fprintf(stderr, "  error:  %s has bad %s CRC (got 0x%08lx, "
              "expected 0x%08lx)\n", filename, chunkstr(chunktyp), calc_crc,
              chunkcrc);
            fflush(stderr);
            return 24;
        }
    } // end of while-loop over chunks

    fclose(infile);

    return 0;   /* success! */

} /* end of function pngsplit() */





/* convert chunk type to character-string "name" (assuming charset is ASCII!) */
static char *chunkstr(ulg typ)
{
    static char str[5];

    str[0] = ((typ >> 24) & 0xff);
    str[1] = ((typ >> 16) & 0xff);
    str[2] = ((typ >>  8) & 0xff);
    str[3] = ((typ      ) & 0xff);
    str[4] = '\0';

    return str;
}




/*
   GRR FIXME:  have bookkeeping error when file already exists:  note name
    mismatch on 2nd-4th file/error messages:

ArcTriomphe-cHRM-red-blue-swap.png:
                                              warning:  ArcTriomphe-cHRM-red-blue-swap.png.0000.sig exists; not overwriting
ArcTriomphe-cHRM-red-green-swap.png:
  write_chunk_to_file() logic error:  OPEN flag set but ArcTriomphe-cHRM-red-blue-swap.png.0000.sig still open
ArcTriomphe-iCCP-red-blue-swap.png:
  write_chunk_to_file() logic error:  OPEN flag set but ArcTriomphe-cHRM-red-blue-swap.png.0000.sig still open
ArcTriomphe-iCCP-red-green-swap.png:
  write_chunk_to_file() logic error:  OPEN flag set but ArcTriomphe-cHRM-red-blue-swap.png.0000.sig still open

 */
// 0 = OK, 1 = warning (exists but continuing), 2 = error
static int
write_chunk_to_file(char *basename, int baselen, ulg num, ulg chunklen, ulg chunktyp, ulg chunkcrc, uch *buf, ulg len, int flags)
{
    static FILE *outfile=NULL;
    static char outname[FNMAX];
    ulg wlen;

    if (flags & WR_OPEN) {
        if (outfile) {
            fprintf(stderr, "  write_chunk_to_file() logic error:  OPEN flag "
              "set but %s still open\n", outname);
            fflush(stderr);
            return 2;
        }

        strncpy(outname, basename, baselen);

        if (num == 0L) {
            strcpy(outname+baselen, ".0000.sig");
        } else {
            sprintf(outname+baselen, ".%04lu.%s", num, chunkstr(chunktyp));
        }

        if (!(flags & WR_FORCE)) {
            if ((outfile = fopen(outname, "rb")) != NULL) {
                fprintf(stderr, "  warning:  %s exists; not overwriting\n",
                  outname);
                fflush(stderr);
                fclose(outfile);
                return 1;
            }
        }

        if ((outfile = fopen(outname, "wb")) == NULL) {
            fprintf(stderr, "  error:  cannot open %s for writing\n", outname);
            fflush(stderr);
            return 2;
        }

        if (num != 0L) {
            fputc((int)((chunklen >> 24) & 0xff), outfile);
            fputc((int)((chunklen >> 16) & 0xff), outfile);
            fputc((int)((chunklen >>  8) & 0xff), outfile);
            fputc((int)((chunklen      ) & 0xff), outfile);

            fputc((int)((chunktyp >> 24) & 0xff), outfile);
            fputc((int)((chunktyp >> 16) & 0xff), outfile);
            fputc((int)((chunktyp >>  8) & 0xff), outfile);
            fputc((int)((chunktyp      ) & 0xff), outfile);
        }
    }

    if (!outfile) {
        fprintf(stderr, "  write_chunk_to_file() logic error:  about to "
          "fwrite() but no file is open\n");
        fflush(stderr);
        return 2;
    }
    wlen = len? fwrite(buf, 1L, len, outfile) : 0L;

    if (flags & WR_CLOSE) {
        if (num != 0L) {
            fputc((int)((chunkcrc >> 24) & 0xff), outfile);
            fputc((int)((chunkcrc >> 16) & 0xff), outfile);
            fputc((int)((chunkcrc >>  8) & 0xff), outfile);
            fputc((int)((chunkcrc      ) & 0xff), outfile);
        }

        if (!outfile) {
            fprintf(stderr, "  write_chunk_to_file() logic error:  CLOSE flag "
              "set but no file is open\n");
            fflush(stderr);
            return 2;
        }

        fclose(outfile);
        outfile = NULL;
        outname[0] = '\0';
    }

    if (wlen != len) {
        fprintf(stderr, "  error:  wrote %lu of %lu bytes (%s)\n", wlen, len,
          outname);
        fflush(stderr);
        return 2;
    }

    return 0;
}
