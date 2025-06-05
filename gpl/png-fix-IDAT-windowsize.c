/* png-fix-IDAT-windowsize.c - simple utility to reset first IDAT's zlib
**                             window-size bytes and fix up CRC to match
**
** Downloads:
**
**      http://gregroelofs.com/greg_software.html
**
** To compile:
**
**	gcc -Wall -O2 -I/path-to-zlib png-fix-IDAT-windowsize.c \
          -o png-fix-IDAT-windowsize -L/path-to-zlib -lz
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
*/

#define VERSION "1.0 of 31 October 2020"

/*
 * TO DO:
 *  - output dir (required) instead of "-fixed.png" name (=> strip existing
 *     path components)
 *  - same timestamp
 *  - summary of how many done (and maybe also "fixing ..." msg)
 *  - own crc32() function => no zlib dependency
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <zlib.h>   // for crc32() only

typedef unsigned long  ulg;
typedef unsigned short ush;
typedef unsigned char  uch;

#define PNG_FIX_IDAT_WINDOWSIZE_USAGE "\
   usage:  png-fix-IDAT-windowsize [options] pngfile [pngfile ...]\n\
   options:\n\
      -force         overwrite existing output files\n\
      -verbose       print more status messages (synonym:  -noquiet)\n\n\
   Uses explicit, hardcoded compression settings and line filters, writing\n\
   result to output file with extension \"-fixed.png\".\n"

#ifndef TRUE
#  define TRUE     1
#endif
#ifndef FALSE
#  define FALSE    0
#endif

#define FNMAX      1024     /* max filename length */
#define BUFSZ      4096

#define MIN_PNG_SIZE (8 + 4+4+13+4 + 4+4+10+4 + 4+4+4) // 67 (IDAT = 10: empir.)

#define U16(x)  ( ((ush)(((uch *)(x))[0]) <<  8) |  \
                  ((ush)(((uch *)(x))[1])      ) )

#define U32(x)  ( ((ulg)(((uch *)(x))[0]) << 24) |  \
                  ((ulg)(((uch *)(x))[1]) << 16) |  \
                  ((ulg)(((uch *)(x))[2]) <<  8) |  \
                  ((ulg)(((uch *)(x))[3])      ) )

uch pngsig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
ulg IHDR = ( (73L << 24) | (72L << 16) | (68L << 8) | (82L) );  // 0x49484452
ulg IDAT = ( (73L << 24) | (68L << 16) | (65L << 8) | (84L) );  // 0x49444154


static int png_fix_IDAT_windowsize (char *filename, int force, int verbose);
static char *chunkstr (ulg typ);



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
            fprintf(stderr, "png-fix-IDAT-windowsize, version %s, by Greg Roelofs.\n",
              VERSION);
            fprintf(stderr, "  This software is licensed under the GNU "
              "General Public License.\n  There is NO warranty.\n");
            fprintf(stderr, "  Compiled with zlib %s; using zlib %s.\n",
              ZLIB_VERSION, zlib_version);
            fprintf(stderr, "\n");
            fprintf(stderr, PNG_FIX_IDAT_WINDOWSIZE_USAGE);
            fflush(stderr);
            return 1;
        }
        ++argn;
    }

    printf("png-fix-IDAT-windowsize, version %s, by Greg Roelofs.\n", VERSION);
    printf("  This software is licensed under the GNU "
      "General Public License.\n  There is NO warranty.\n");
    printf("  Compiled with zlib %s; using zlib %s.\n",
      ZLIB_VERSION, zlib_version);
    printf("\n");
    fflush(stdout);

    if ( argn == argc ) {
        fprintf(stderr, PNG_FIX_IDAT_WINDOWSIZE_USAGE);
        fflush(stderr);
        return 5;
    } else {
        filename = argv[argn];
        ++argn;
    }


    /*=============================  MAIN LOOP  =============================*/

    while (argn <= argc) {
        int retval;

        if (verbose) {
            printf("%s:\n", filename);
            fflush(stdout);
        }

        retval = png_fix_IDAT_windowsize(filename, force, verbose);

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
            printf("There were errors fixing %d PNG file%s out of a"
              " total of %d file%s.\n",
              error_count, (error_count == 1)? "" : "s",
              file_count, (file_count == 1)? "" : "s");
        else
            printf("No errors detected while fixing %d PNG image%s.\n",
              file_count, (file_count == 1)? "" : "s");
        fflush(stdout);
    }

    return latest_error;
}




static int png_fix_IDAT_windowsize(char *filename, int force, int verbose)
{
    FILE *infile, *outfile;
    uch inbuf[BUFSZ], *inptr, *endptr;
    uch *cbuf=NULL, *cptr;
    char outname[FNMAX];
    static char *colortype_name[] = {
        "grayscale", "[INVALID]", "RGB", "palette",
        "gray+alpha", "[INVALID]", "RGBA"
    };
    int fnlen, incnt;
    int depth, colortype, /* compress_method, filter_method, */ interlaced;
    int channels, bitsperpixel, error;
    int haveIDAT, haveEOF;
    long width, height, bitwidth, bytewidth;
    long chunklen, /* numfilters, */ ucsize;
    //long csize, csize_orig;
    ulg chunktyp, chunkcrc, calc_crc;
    ulg file_offset, file_offset_IDATs=0L;
    ulg bytes_remaining;


    if ((infile = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "  error:  cannot open %s for reading\n", filename);
        fflush(stderr);
        return 2;
    }

    /* build the output filename from the input name by inserting "-fixed"
     * before the ".png" extension (or by appending that plus ".png" if
     * there isn't any extension), then make sure it doesn't exist already */

    fnlen = strlen(filename);

    if (fnlen > FNMAX-9) {
        fprintf(stderr,
          "  warning:  base filename [%s] will be truncated\n", filename);
        fflush(stderr);
        fnlen = FNMAX-9;
    }
    strncpy(outname, filename, fnlen);
    if (strncmp(outname+fnlen-4, ".png", 4) == 0)
        strcpy(outname+fnlen-4, "-fixed.png");
    else
        strcpy(outname+fnlen, "-fixed.png");
    if (!force) {
        if ((outfile = fopen(outname, "rb")) != NULL) {
            fprintf(stderr, "  error:  %s exists; not overwriting\n",
              outname);
            fflush(stderr);
            fclose(outfile);
            return 15;
        }
    }


    /*
    ** Step 1: read in the input image.
    */

    // check PNG header
    inptr = endptr = inbuf;
    incnt = fread(endptr, 1, BUFSZ, infile);   // 4096 bytes
    endptr += incnt;

    if (incnt < MIN_PNG_SIZE || memcmp(inbuf, pngsig, 8) != 0) {
        fprintf(stderr, "  error:  %s is not a PNG file\n", filename);
        fflush(stderr);
        return 17;
    }
    inptr += 8;

    // check IHDR length, name/ID bytes, and CRC over data
    chunklen = U32(inptr);
    inptr += 4;
    chunktyp = U32(inptr);
    if (chunklen != 13 || chunktyp != IHDR) {
        fprintf(stderr, "  error:  %s has bad IHDR chunk\n", filename);
        fflush(stderr);
        return 18;
    }
    //inptr += 4;   still pointing at chunk name/ID for now
    calc_crc = crc32(0L, Z_NULL, 0);
    calc_crc = crc32(calc_crc, inptr, 4+chunklen);
    inptr += 4;   // now pointing at start of chunk data
    chunkcrc = U32(inptr+chunklen);
    if (calc_crc != chunkcrc) {
        fprintf(stderr, "  error:  %s has bad IHDR CRC (got 0x%08lx, "
          "expected 0x%08lx)\n", filename, calc_crc, chunkcrc);
        fflush(stderr);
        return 19;
    }

    // store IHDR data
    width = U32(inptr);
    inptr += 4;
    height = U32(inptr);
    inptr += 4;
    depth = *inptr++;
    colortype = *inptr++;
    ++inptr; //compress_method = *inptr++;
    ++inptr; //filter_method = *inptr++;
    interlaced = *inptr++;
    inptr += 4;  // skip over IHDR CRC
    incnt = endptr - inptr;
    file_offset = inptr - inbuf;

    if (width <= 0 || height <= 0) {
        fprintf(stderr, "  error:  %s has invalid dimensions (%ld x %ld)\n",
          filename, width, height);
        fflush(stderr);
        return 20;
    }

    error = 0;

    switch (colortype) {
      case 0:
        channels = 1;
        if (depth != 1 && depth !=2 && depth != 4 && depth != 8 && depth != 16)
            ++error;
        break;

      case 2:
        channels = 3;
        if (depth != 8 && depth != 16)
            ++error;
        break;

      case 3:
        channels = 1;
        if (depth != 1 && depth !=2 && depth != 4 && depth != 8)
            ++error;
        break;

      case 4:
        channels = 2;
        if (depth != 8 && depth != 16)
            ++error;
        break;

      case 6:
        channels = 4;
        if (depth != 8 && depth != 16)
            ++error;
        break;

      default:
        fprintf(stderr, "  error:  %s has invalid colortype (%d)\n", filename,
          colortype);
        fflush(stderr);
        return 22;
    }

    if (error) {
        fprintf(stderr, "  error:  %s has invalid sample depth (%d) for "
          "colortype (%d = %s)\n", filename, depth, colortype,
          colortype_name[colortype]);
        fflush(stderr);
        return 23;
    }

    bitsperpixel = depth * channels;  /* can't overflow */

    if (verbose) {
        printf("  %ldx%ld, %d-bit, %sinterlaced, %s (type %d) image\n",
          width, height, bitsperpixel, interlaced? "" : "non-",
          colortype_name[colortype], colortype);
    }


    /* Calculate uncompressed image size and check for overflows.  Strictly
     * speaking, this doesn't apply to interlaced images, but since no
     * interlace-pass subimage can be bigger than the complete image, the
     * latter checks are sufficient. */

    /* (this calculation works for both sub-8-bit and 8-bit or greater) */
    bitwidth = width * bitsperpixel;
    bytewidth = 1 + (bitwidth + 7) / 8;   // 1 -> row-filter byte
    ucsize = bytewidth * height;
    //numfilters = height;

    if (bitwidth/width != bitsperpixel || bytewidth <= 0 ||
        ucsize/bytewidth != height)
    {
        fprintf(stderr, "  error:  %s has invalid dimensions\n", filename);
        fflush(stderr);
        return 20;
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
        inptr = endptr = inbuf;						\
        incnt = fread(endptr, 1, BUFSZ, infile);   /* 4096 bytes */	\
        endptr += incnt;						\
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

    /* Next loop over remaining chunks, verifying CRC of each but otherwise
     * ignoring them.  Stop as soon as find first IDAT. */

//inptr currently points at "length" bytes of first chunk after IHDR

    //csize_orig = 0L;
    error = haveEOF = haveIDAT = FALSE;

    while (!error && !haveEOF) {
        // check chunk length, name/ID bytes, and CRC over data
        GET_U32(chunklen, 0)  // this advances inptr, refills buffer as needed
        calc_crc = crc32(0L, Z_NULL, 0);
        GET_U32(chunktyp, 1)  // ...and also updates calc_crc in this case

        // NOTE:  The displayed file offset is that of the chunk name/ID,
        //        *not* the true beginning of the chunk (length-bytes,
        //        4 bytes earlier).  This matches pngcheck's behavior.
        if (verbose) {
            printf("    %s chunk (0x%lx), length %ld, at file offset %lu "
              "(0x%05lx)\n", chunkstr(chunktyp), chunktyp, chunklen,
              file_offset-4, file_offset-4);
        }

        if (chunktyp == IDAT) {
            haveIDAT = TRUE;
            file_offset_IDATs = file_offset - 8;     // start of chunklen
            //csize_orig += chunklen;
            break;
        }

        // now pointing at chunk data (i.e., AFTER chunk name/ID)
        while (chunklen > incnt) {
            calc_crc = crc32(calc_crc, inptr, incnt);
            chunklen -= incnt;
            file_offset += incnt;

            /* read more bytes into buffer */
            inptr = inbuf;
            incnt = fread(inbuf, 1, BUFSZ, infile);   /* 4096 bytes */
            if (incnt <= 0) {
                fprintf(stderr, "  error:  unexpected EOF while reading %s "
                  "(chunk is missing %ld bytes)\n", filename, chunklen);
                fflush(stderr);
                error = haveEOF = TRUE;
                break;
            }
            endptr = inbuf + incnt;   // rarely used, but maybe sometimes?
        }
        if (error)
            break;
        calc_crc = crc32(calc_crc, inptr, chunklen);
        inptr += chunklen;
        incnt -= chunklen;
        file_offset += chunklen;

        GET_U32(chunkcrc, 0)
        if (calc_crc != chunkcrc) {
            fprintf(stderr, "  error:  %s has bad %s CRC (got 0x%08lx, "
              "expected 0x%08lx)\n", filename, chunkstr(chunktyp), chunkcrc,
              chunkcrc);
            fflush(stderr);
            error = TRUE;
            break;
        }

    } // end of while-loop over post-IHDR chunks (first pass)

    if (!haveIDAT) {
        fprintf(stderr, "  error:  found no IDAT chunks in %s\n", filename);
        fflush(stderr);
        error = TRUE;
    }

    if (error) {
        fclose(infile);
        return 24;
    }

    /* end of "sniffer" pass; now read first IDAT into buffer */

    if (fseek(infile, file_offset_IDATs, SEEK_SET) < 0) {
        fprintf(stderr, "  error:  %s: can't seek back to start of IDATs?!\n",
          filename);
        fflush(stderr);
        fclose(infile);
        return 25;
    }

    // allocate compressed-image buffer (unless more than 10 MB?)
    if ((cbuf = (uch *)malloc(chunklen+12)) == NULL) {
        fprintf(stderr, "  error:  %s: can't allocate buffer for first IDAT\n",
          filename);
        fflush(stderr);
        fclose(infile);
        return 26;
    }

    // second pass through file:  loop over IDATs, reading data into cbuf

    file_offset = file_offset_IDATs;
    cptr = cbuf;
    //csize = csize_orig;

    incnt = fread(cbuf, 1, chunklen+12, infile);

    calc_crc = crc32(0L, Z_NULL, 0);
    calc_crc = crc32(calc_crc, cbuf+4, chunklen+4);
    chunkcrc = U32(cbuf+4 + chunklen+4);
    if (calc_crc != chunkcrc) {
        fprintf(stderr, "  error:  %s has bad %s CRC (got 0x%08lx, "
          "expected 0x%08lx)\n", filename, chunkstr(chunktyp), chunkcrc,
          chunkcrc);
        fflush(stderr);
        fclose(infile);
        return 23;
    }

    cptr = cbuf + 8;
//  if (cptr[0] == 0x68 && cptr[1] == 0x81)     // (overly) conservative check
    if (cptr[0] != 0x78 || cptr[1] != 0x9c)     // aggressive check
    {
        cptr[0] = 0x78;
        cptr[1] = 0x9c;
    } else {
        fprintf(stderr, "  note:  %s does not appear to have bad zlib "
          "windowBits; skipping\n", filename);
        fflush(stderr);
        fclose(infile);
        return 0;
    }
    calc_crc = crc32(0L, Z_NULL, 0);
    calc_crc = crc32(calc_crc, cbuf+4, chunklen+4);
    cptr = cbuf+4 + chunklen+4;
    cptr[0] = (calc_crc >> 24);
    cptr[1] = (calc_crc >> 16) & 0xff;
    cptr[2] = (calc_crc >>  8) & 0xff;
    cptr[3] = (calc_crc      ) & 0xff;

    if ((outfile = fopen(outname, "wb")) == NULL) {
        fprintf(stderr, "  error:  cannot open %s for writing\n", outname);
        fflush(stderr);
        return 16;
    }


    /* Now copy infile up to first IDAT; copy cbuf (replaced IDAT); and finally
     * copy infile from end of first IDAT to end of file. */

    // copy infile to outfile, up to file_offset_IDATs
    fseek(infile, 0, SEEK_SET);
    bytes_remaining = file_offset_IDATs;
    while (bytes_remaining > 0) {
        /* read more bytes into buffer */
        incnt = (bytes_remaining > BUFSZ)? BUFSZ : bytes_remaining;
        incnt = fread(inbuf, 1, incnt, infile);   /* 4096 bytes (or less) */
        if (incnt <= 0) {
            fprintf(stderr, "  error:  unexpected EOF while copying %s\n",
              filename);
            fflush(stderr);
            error = haveEOF = TRUE;
            break;
        }
        fwrite(inbuf, 1, incnt, outfile);
        bytes_remaining -= incnt;
    }

    // copy modified IDAT (cbuf) to outfile
    fwrite(cbuf, 1, chunklen+12, outfile);

    // copy rest of infile to outfile
    fseek(infile, file_offset_IDATs + chunklen+12, SEEK_SET);
    while (!haveEOF) {
        /* read more bytes into buffer */
        incnt = fread(inbuf, 1, BUFSZ, infile);   /* 4096 bytes */
        if (incnt <= 0) {
            // apparently we're done
            haveEOF = TRUE;
            break;
        }
        fwrite(inbuf, 1, incnt, outfile);
    }

    fclose(outfile);

    if (cbuf) {
        free(cbuf);
        cbuf = NULL;
    }

    fclose(infile);

    return error? 27 : 0;

} /* end of function png_fix_IDAT_windowsize() */





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
