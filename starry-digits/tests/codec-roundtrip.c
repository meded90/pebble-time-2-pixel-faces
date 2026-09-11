#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/c/intro_codec.h"
#include "../src/c/intro_config.h"
static unsigned u32(const unsigned char *p) { return p[0]|p[1]<<8|p[2]<<16|p[3]<<24; }
int main(int argc,char **argv) {
  assert(argc==3); FILE *f=fopen(argv[1],"rb"),*raw=fopen(argv[2],"rb"); assert(f&&raw);
  fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);unsigned char *b=malloc(n);assert(fread(b,1,n,f)==(size_t)n);fclose(f);
  unsigned char pixels[204*228],expected[45600],scratch[INTRO_MAX_DELTA];memset(pixels,192,sizeof(pixels));
  for(unsigned i=0;i<INTRO_FRAMES;i++){
    unsigned a=u32(b+i*4),z=u32(b+i*4+4);assert(a<z&&z<=(unsigned)n);
    assert(intro_decode(b+a,z-a,scratch,sizeof(scratch),pixels,204));
    assert(fread(expected,1,sizeof(expected),raw)==sizeof(expected));
    for(unsigned y=0;y<228;y++){assert(!memcmp(pixels+y*204,expected+y*200,200));for(unsigned x=200;x<204;x++)assert(pixels[y*204+x]==192);}
    assert(!intro_decode(b+a,(z-a)/2,scratch,sizeof(scratch),pixels,204));
    // Restore the expected previous frame after intentionally malformed input.
    for(unsigned y=0;y<228;y++)memcpy(pixels+y*204,expected+y*200,200);
  }
  unsigned char invalid[]={1,0,0};assert(!intro_decode(invalid,sizeof(invalid),scratch,sizeof(scratch),pixels,204));
  assert(!intro_decode(b+u32(b),u32(b+4)-u32(b),scratch,1,pixels,204));
  assert(!intro_decode(b+u32(b),u32(b+4)-u32(b),scratch,sizeof(scratch),pixels,199));
  free(b);fclose(raw);puts("PASS: every real frame matches losslessly; row padding preserved; truncated/invalid/capacity errors rejected");
}
