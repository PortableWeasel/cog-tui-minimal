#define _XOPEN_SOURCE 700
#include <cog_tui/markdown.h>
#include <assert.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static uint32_t random_word(uint32_t *state) { *state=*state*1664525u+1013904223u;return *state; }
int main(void)
{
    assert(setlocale(LC_CTYPE,"C.UTF-8")!=NULL);
    cog_tui_markdown_t *md=NULL;assert(cog_tui_markdown_new(&md,NULL)==COG_OK);
    uint32_t random=7;char input[2048],copy[2048];clock_t start=clock();size_t renders=0;
    static const char alphabet[]="*`~_|-[]()\0\\\n\t abc0123\x1b\x7f\xc2\xe4\xb8\xad\xf0\x9f\x80";
    for(size_t iteration=0;iteration<1500;iteration++) {
        size_t n=random_word(&random)%sizeof input;
        for(size_t i=0;i<n;i++)input[i]=alphabet[random_word(&random)%(sizeof alphabet-1)];
        memcpy(copy,input,n);
        cog_tui_md_source_t source={input,n,1,(cog_tui_md_phase_t)(iteration%4)};
        cog_tui_md_window_t window={1+random_word(&random)%100,iteration%2?SIZE_MAX:0,1+random_word(&random)%20};
        cog_tui_md_frame_t *frame=NULL;
        cog_result_t result=cog_tui_markdown_render(md,&source,COG_TUI_MD_FORMAT_MARKDOWN,&window,&frame);
        assert(result==COG_OK || result==COG_ERROR_LIMIT || result==COG_ERROR_PROTOCOL);
        if(result!=COG_OK)assert(cog_tui_markdown_render(md,&source,COG_TUI_MD_FORMAT_PLAIN,&window,&frame)==COG_OK);
        cog_tui_md_view_t v;assert(cog_tui_md_frame_view(frame,&v)==COG_OK);
        for(size_t i=0;i<v.row_count;i++)for(size_t j=0;j<v.rows[i].run_count;j++) {
            const cog_tui_md_run_t *run=&v.rows[i].runs[j];assert(run->column<=v.columns && run->cells<=v.columns-run->column);
            for(size_t k=0;k<run->bytes;k++)assert((unsigned char)run->text[k]>=32 && (unsigned char)run->text[k]!=127);
        }
        assert(memcmp(input,copy,n)==0);assert(cog_tui_md_frame_free(&frame)==COG_OK);renders++;
    }
    printf("%zu deterministic arbitrary-byte renders: %.3f ms mean\n",renders,1000.0*(double)(clock()-start)/(double)CLOCKS_PER_SEC/(double)renders);
    char large[131072];
    static const char *patterns[]={"*","a*","[x](", "plain words "};
    for(size_t corpus=0;corpus<sizeof patterns/sizeof patterns[0];corpus++) {
        size_t len=strlen(patterns[corpus]);for(size_t i=0;i<sizeof large;i++)large[i]=patterns[corpus][i%len];
        cog_tui_md_source_t source={large,sizeof large,1,COG_TUI_MD_STREAMING};cog_tui_md_window_t window={80,SIZE_MAX,24};start=clock();
        for(size_t i=0;i<10;i++) {
            cog_tui_md_frame_t *frame=NULL;cog_result_t result=cog_tui_markdown_render(md,&source,COG_TUI_MD_FORMAT_MARKDOWN,&window,&frame);
            assert(result==COG_OK || result==COG_ERROR_LIMIT);
            if(result==COG_ERROR_LIMIT)assert(cog_tui_markdown_render(md,&source,COG_TUI_MD_FORMAT_PLAIN,&window,&frame)==COG_OK);
            assert(cog_tui_md_frame_free(&frame)==COG_OK);
        }
        printf("128KiB corpus %zu render: %.3f ms mean\n",corpus,1000.0*(double)(clock()-start)/(double)CLOCKS_PER_SEC/10.0);
    }
    /* Regression: overflow preflight must not rescan all source per table. */
    const char *table="|a|\n|-|\n|b|\n\n";size_t table_len=strlen(table),many_bytes=4000*table_len;
    assert(many_bytes<=sizeof large);for(size_t i=0;i<4000;i++)memcpy(large+i*table_len,table,table_len);
    cog_tui_md_source_t many={large,many_bytes,1,COG_TUI_MD_STREAMING};cog_tui_md_window_t tail={80,SIZE_MAX,24};
    cog_tui_md_frame_t *many_frame=NULL;start=clock();
    assert(cog_tui_markdown_render(md,&many,COG_TUI_MD_FORMAT_MARKDOWN,&tail,&many_frame)==COG_OK);
    printf("4000-table corpus (%zu bytes): %.3f ms\n",many_bytes,1000.0*(double)(clock()-start)/(double)CLOCKS_PER_SEC);
    cog_tui_md_view_t many_view;assert(cog_tui_md_frame_view(many_frame,&many_view)==COG_OK && many_view.total_rows==16000);
    assert(cog_tui_md_frame_free(&many_frame)==COG_OK);
    assert(cog_tui_markdown_free(&md)==COG_OK);return 0;
}
