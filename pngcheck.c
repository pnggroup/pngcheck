/*
 * authenticate a PNG file (as per draft 9)
 *
 * this program checks the PNG identifier with conversion checks,
 * the file structure and the chunk CRCs.
 * With -v switch, the chunk names are printed.
 *
 * written by Alexander Lehmann <alex@hal.rhein-main.de>
 *
 *
 * 95.02.23 fixed wrong magic numbers
 *
 * 95.03.13 crc code from png spec, compiles on memory impaired PCs now,
 *          check for IHDR/IEND chunks
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BS 32000 /* size of read block for CRC calculation */
int verbose; /* ==1 print chunk info */
char *fname;
char buffer[BS];

/* table of crc's of all 8-bit messages */
unsigned long crc_table[256];

/* Flag: has the table been computed? Initially false. */
int crc_table_computed = 0; 

/* make the table for a fast crc */
void make_crc_table(void)
{
  unsigned long c;
  int n, k;
 
  for (n = 0; n < 256; n++)
  {
    c = (unsigned long)n;
    for (k = 0; k < 8; k++)
      c = c & 1 ? 0xedb88320L ^ (c >> 1) : c >> 1;
    crc_table[n] = c;
  }
  crc_table_computed = 1;
}

/* update a running crc with the bytes buf[0..len-1]--the crc should be
   initialized to all 1's, and the transmitted value is the 1's complement
   of the final running crc (see the crc() routine below)). */

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

long getlong(FILE *fp)
{
  long res=0;
  int c;
  int i;

  for(i=0;i<4;i++) {
    if((c=fgetc(fp))==EOF) {
      printf("%s: EOF while reading 4 bytes value\n", fname);
      return 0;
    }
    res<<=8;
    res|=c&0xff;
  }
  return res;
}

void pngcheck(FILE *fp, char *_fname)
{
  long s;
  char magic[9];
  int toread;
  int c;
  unsigned long crc;
  int first=0;

  fname=_fname;

  if(fread(magic, 1, 9, fp)!=9) {
    printf("%s: Cannot read PNG header\n", fname);
    return;
  }

  ungetc(magic[8], fp);

  if(strncmp(magic+1, "PNG", 3)!=0) {
    printf("%s: not a PNG file\n", fname);
    return;
  }

  /* this is probably bad C style, but it saves a lot of typing */
#define CHECK(str, type)                                                 \
  if(strncmp(magic+4, str, sizeof(str)-1)==0) {                          \
    printf("%s: seems to have suffered " type " conversion\n", fname);   \
    return;                                                              \
  }

  CHECK("\n\032", "DOS->unix")
  CHECK("\r\032", "DOS->Mac")  
  CHECK("\r\r\032", "unix->Mac")
  CHECK("\n\n\032", "Mac->unix")
  CHECK("\r\n\032\r\n", "unix->DOS")
  CHECK("\r\r\n\032", "unix->DOS")

  if(strncmp(magic+4, "\r\n\032\n", 4)!=0) {
    printf("%s: seems to have suffered some kind of EOL conversion\n", fname);
    return;
  }

  if(magic[0]=='\011') {
    printf("%s: was probably transmitted through a 7bit channel\n", fname);
    return;
  }

  if(magic[0]!='\211') {
    printf("%s: was probably transmitted in text mode\n", fname);
    return;
  }

  while((c=fgetc(fp))!=EOF) {
    ungetc(c, fp);
    s=getlong(fp);
    if(fread(magic, 1, 4, fp)!=4) {
      printf("%s: EOF while reading chunk type\n", fname);
      return;
    }
    if(!isalpha(magic[0]) || !isalpha(magic[1]) || !isalpha(magic[2]) ||
       !isalpha(magic[3])) {
      printf("%s: chunk type doesn't comply to naming rules\n", fname);
      return;
    }
    magic[4]=0;

    if(verbose) {
      printf("%s: chunk %s at %lx length %lx\n", fname, magic, ftell(fp)-4, s);
    }

    if(first && strcmp(magic,"IHDR")!=0) {
      printf("%s: file doesn't start with a IHDR chunk\n", fname);
    }
    first=0;

    crc=update_crc(0xffffffff,magic,4);

    while(s>0) {
      toread=s;
      if(toread>BS) {
        toread=BS;
      }
      if(fread(buffer, 1, toread, fp)!=toread) {
        printf("%s: EOF while reading chunk data (%s)\n", fname, magic);
	return;
      }
      crc=update_crc(crc,buffer, toread);
      s-=toread;
    }
    if(getlong(fp)!=~crc) {
      printf("%s: CRC error in chunk %s\n", fname, magic);
      return;
    }
  }
  if(strcmp(magic, "IEND")!=0) {
    printf("%s: file doesn't end with a IEND chunk\n", fname);
    return;
  }
  printf("%s: file appears to be OK\n", fname);
}

int main(int argc, char *argv[])
{
  FILE *fp;
  int i;

  if(argc>1 && strcmp(argv[1],"-v")==0) {
    verbose=1;
    argc--;
    argv++;
  }

  if(argc==1) {
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
