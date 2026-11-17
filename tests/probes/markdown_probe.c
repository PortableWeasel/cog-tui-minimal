/* Throwaway evidence harness: no application API. */
#include "md4c.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
typedef struct { unsigned code, table, strong; size_t text; } probe;
static int block(MD_BLOCKTYPE t, void *d, void *u) { probe *p=u; (void)d; if(t==MD_BLOCK_CODE) p->code++; if(t==MD_BLOCK_TABLE) p->table++; return 0; }
static int leave(MD_BLOCKTYPE t, void *d, void *u) { (void)t;(void)d;(void)u;return 0; }
static int span(MD_SPANTYPE t, void *d, void *u) { probe *p=u;(void)d;if(t==MD_SPAN_STRONG)p->strong++;return 0; }
static int endspan(MD_SPANTYPE t, void *d, void *u) { (void)t;(void)d;(void)u;return 0; }
static int text_cb(MD_TEXTTYPE t, const MD_CHAR *s, MD_SIZE n, void *u) { probe *p=u;(void)t;(void)s;p->text+=n;return 0; }
static probe parse(const char *s,size_t n) { probe p={0}; MD_PARSER parser={0};parser.flags=MD_FLAG_TABLES|MD_FLAG_NOHTMLBLOCKS|MD_FLAG_NOHTMLSPANS;parser.enter_block=block;parser.leave_block=leave;parser.enter_span=span;parser.leave_span=endspan;parser.text=text_cb;assert(md_parse(s,(MD_SIZE)n,&parser,&p)==0);return p; }
int main(void) {
 const char *fixtures[]={"```c\nhello", "| a | b |\n| --- | --- |\n| x |", "**open", "[hello][x]\n\n[x]: https://example.com", "|a|b|\n|-|-|\n|x|y|OVERFLOW|", "a\\|b", "\xf0\x9f\x98\x80"};
 for(size_t i=0;i<sizeof fixtures/sizeof fixtures[0];i++)for(size_t n=0;n<=strlen(fixtures[i]);n++)(void)parse(fixtures[i],n);
 assert(parse(fixtures[0],strlen(fixtures[0])).code==1);
 assert(parse(fixtures[1],strlen(fixtures[1])).table==1);
 assert(parse(fixtures[2],strlen(fixtures[2])).strong==0);
 probe overflow=parse(fixtures[4],strlen(fixtures[4]));
 printf("overflow table callback text bytes=%zu (extra cell dropped by parser)\n",overflow.text);
 char large[131072];memset(large,'*',sizeof large);clock_t start=clock();for(unsigned i=0;i<100;i++)(void)parse(large,sizeof large);
 printf("128KiB adversarial delimiter parse mean %.3fms; prefix matrix passed\n",1000.0*(double)(clock()-start)/(double)CLOCKS_PER_SEC/100.0);
 return 0;
}
