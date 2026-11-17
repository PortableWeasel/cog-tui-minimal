#define _XOPEN_SOURCE 700
#include <cog_tui/markdown.h>
#include <assert.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Linker wrappers apply to both UI adapter and pinned MD4C. */
void *__real_malloc(size_t n);
void *__real_calloc(size_t n,size_t size);
void *__real_realloc(void *p,size_t n);
void *__wrap_malloc(size_t n);
void *__wrap_calloc(size_t n,size_t size);
void *__wrap_realloc(void *p,size_t n);
static size_t calls,fail_at;
static bool enabled;
static bool fail(void) { if(!enabled)return false;return ++calls==fail_at; }
void *__wrap_malloc(size_t n) { return fail()?NULL:__real_malloc(n); }
void *__wrap_calloc(size_t n,size_t size) { return fail()?NULL:__real_calloc(n,size); }
void *__wrap_realloc(void *p,size_t n) { return fail()?NULL:__real_realloc(p,n); }
int main(void)
{
    assert(setlocale(LC_CTYPE,"C.UTF-8")!=NULL);
    cog_tui_markdown_t *md=NULL;assert(cog_tui_markdown_new(&md,NULL)==COG_OK);
    const char *s="# Heading\n\n| a | b |\n| --- | --- |\n| wrapped long cell | value |\n\n```c\nint x;\n```\n\n**bold** [link](https://example.com)";
    cog_tui_md_source_t source={s,strlen(s),1,COG_TUI_MD_STREAMING};cog_tui_md_window_t window={30,SIZE_MAX,20};size_t cases=0;
    for(size_t i=1;i<10000;i++) {
        cog_tui_md_frame_t *frame=NULL;calls=0;fail_at=i;enabled=true;
        cog_result_t result=cog_tui_markdown_render(md,&source,COG_TUI_MD_FORMAT_MARKDOWN,&window,&frame);
        enabled=false;
        assert(result==COG_OK || result==COG_ERROR_NOMEM || result==COG_ERROR_PROTOCOL);
        if(result==COG_OK) { assert(frame!=NULL);assert(cog_tui_md_frame_free(&frame)==COG_OK); }
        else assert(frame==NULL);
        cases++;
        if(i>calls)break;
    }
    assert(cases<10000);assert(cog_tui_markdown_free(&md)==COG_OK);
    printf("%zu allocation-failure points tested (adapter and parser)\n",cases);return 0;
}
