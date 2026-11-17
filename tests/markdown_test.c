#define _XOPEN_SOURCE 700
#include <cog_tui/markdown.h>
#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static cog_tui_md_frame_t *render(cog_tui_markdown_t *md,const char *s,size_t n,cog_tui_md_phase_t phase,size_t width,size_t first,size_t rows,cog_tui_md_format_t format)
{
    cog_tui_md_source_t source={s,n,7,phase};cog_tui_md_window_t window={width,first,rows};cog_tui_md_frame_t *f=NULL;
    cog_result_t r=cog_tui_markdown_render(md,&source,format,&window,&f);
    if(format==COG_TUI_MD_FORMAT_MARKDOWN && (r==COG_ERROR_PROTOCOL || r==COG_ERROR_LIMIT))
        r=cog_tui_markdown_render(md,&source,COG_TUI_MD_FORMAT_PLAIN,&window,&f);
    if(r!=COG_OK)fprintf(stderr,"render failed %d input %.*s\n",r,(int)n,s);
    assert(r==COG_OK && f!=NULL);return f;
}
static cog_tui_md_view_t view(cog_tui_md_frame_t *f)
{
    cog_tui_md_view_t v;assert(cog_tui_md_frame_view(f,&v)==COG_OK);return v;
}
static void check(cog_tui_md_frame_t *f)
{
    size_t storage=0;assert(cog_tui_md_frame_storage(f,&storage)==COG_OK && storage>0 && storage<=4194304);
    cog_tui_md_view_t v=view(f);assert(v.row_count<=300 && v.columns<=1000 && v.pending_utf8_bytes<=3);
    for(size_t i=0;i<v.row_count;i++) {
        size_t col=0;
        for(size_t j=0;j<v.rows[i].run_count;j++) {
            const cog_tui_md_run_t *r=&v.rows[i].runs[j];assert(r->column==col && r->cells<=v.columns-col);col+=r->cells;
            for(size_t k=0;k<r->bytes;k++)assert((unsigned char)r->text[k]>=32 && (unsigned char)r->text[k]!=127);
            if(v.phase!=COG_TUI_MD_STREAMING)assert(r->origin!=COG_TUI_MD_PREVIEW);
        }
    }
}
static void equal(cog_tui_md_frame_t *a,cog_tui_md_frame_t *b)
{
    cog_tui_md_view_t x=view(a),y=view(b);assert(x.row_count==y.row_count && x.total_rows==y.total_rows && x.pending_utf8_bytes==y.pending_utf8_bytes);
    for(size_t i=0;i<x.row_count;i++) {
        assert(x.rows[i].run_count==y.rows[i].run_count);
        for(size_t j=0;j<x.rows[i].run_count;j++) {
            const cog_tui_md_run_t *p=&x.rows[i].runs[j],*q=&y.rows[i].runs[j];
            assert(p->bytes==q->bytes && p->cells==q->cells && p->style==q->style && p->origin==q->origin && memcmp(p->text,q->text,p->bytes)==0);
        }
    }
}
static void flatten(cog_tui_md_frame_t *f,char *out,size_t cap)
{
    cog_tui_md_view_t v=view(f);size_t n=0;
    for(size_t i=0;i<v.row_count;i++) { for(size_t j=0;j<v.rows[i].run_count;j++) { const cog_tui_md_run_t *r=&v.rows[i].runs[j];assert(r->bytes<cap-n);memcpy(out+n,r->text,r->bytes);n+=r->bytes; }if(i+1<v.row_count) { assert(n+1<cap);out[n++]='\n'; } }out[n]='\0';
}
int main(void)
{
    assert(setlocale(LC_CTYPE,"C.UTF-8")!=NULL);
    cog_tui_markdown_t *md=NULL;assert(cog_tui_markdown_new(&md,NULL)==COG_OK);
    const char *fixtures[]={"", "# Heading\n\n**bold** and *italic* and `code`.", "```c\nint x = 1;", "```\n**not bold**\n``\nthen", "~~~x\nfoo\n~~~\nprose", "| a | b |\n| --- | --- |\n| first | value |\n| partial |", "| a | b |\n| --", "[ref][x]\n\n[x]: https://example.com", "[incomplete](http:", "***a*** \\* literal", "> quote\n> - list\n>   - child", "中文 cafe\xcc\x81 and \xf0\x9f\x98\x80", "&amp; &#27; <script>literal</script>", "a\x1b[31m\x7f"};
    for(size_t f=0;f<sizeof fixtures/sizeof fixtures[0];f++) {
        const char *s=fixtures[f];size_t n=strlen(s);char copy[1024];assert(n<sizeof copy);memcpy(copy,s,n);
        for(size_t width=1;width<=80;width+=13) {
            for(size_t i=0;i<=n;i++) {
                cog_tui_md_frame_t *a=render(md,s,i,COG_TUI_MD_STREAMING,width,SIZE_MAX,12,COG_TUI_MD_FORMAT_MARKDOWN);check(a);
                cog_tui_markdown_t *fresh=NULL;assert(cog_tui_markdown_new(&fresh,NULL)==COG_OK);
                cog_tui_md_frame_t *b=render(fresh,s,i,COG_TUI_MD_STREAMING,width,SIZE_MAX,12,COG_TUI_MD_FORMAT_MARKDOWN);equal(a,b);
                assert(cog_tui_md_frame_free(&a)==COG_OK);assert(cog_tui_md_frame_free(&b)==COG_OK);assert(cog_tui_markdown_free(&fresh)==COG_OK);
            }
            for(cog_tui_md_phase_t phase=COG_TUI_MD_COMPLETE;phase<=COG_TUI_MD_FAILED;phase++) {
                cog_tui_md_frame_t *a=render(md,s,n,phase,width,0,300,COG_TUI_MD_FORMAT_MARKDOWN);check(a);assert(!view(a).preview_applied);
                cog_tui_markdown_t *fresh=NULL;assert(cog_tui_markdown_new(&fresh,NULL)==COG_OK);
                cog_tui_md_frame_t *once=render(fresh,s,n,phase,width,0,300,COG_TUI_MD_FORMAT_MARKDOWN);equal(a,once);
                assert(cog_tui_md_frame_free(&once)==COG_OK);assert(cog_tui_markdown_free(&fresh)==COG_OK);
                assert(cog_tui_md_frame_free(&a)==COG_OK);
            }
        }
        assert(memcmp(copy,s,n)==0);
    }
    const char *styled="# Head\n\n**bold** *em* `code`";
    cog_tui_md_frame_t *styled_frame=render(md,styled,strlen(styled),COG_TUI_MD_COMPLETE,80,0,20,COG_TUI_MD_FORMAT_MARKDOWN);
    cog_tui_md_view_t styled_view=view(styled_frame);unsigned mask=0;
    for(size_t i=0;i<styled_view.row_count;i++)for(size_t j=0;j<styled_view.rows[i].run_count;j++)mask|=styled_view.rows[i].runs[j].style;
    assert((mask&(COG_TUI_MD_BOLD|COG_TUI_MD_ITALIC|COG_TUI_MD_CODE))==(COG_TUI_MD_BOLD|COG_TUI_MD_ITALIC|COG_TUI_MD_CODE));
    assert(cog_tui_md_frame_free(&styled_frame)==COG_OK);
    styled_frame=render(md,"```\nopen",8,COG_TUI_MD_STREAMING,40,0,20,COG_TUI_MD_FORMAT_MARKDOWN);
    assert(view(styled_frame).preview_applied);assert(cog_tui_md_frame_free(&styled_frame)==COG_OK);
    char out[8192];cog_tui_md_frame_t *f=render(md,"one\ntwo\nthree\nfour\n",19,COG_TUI_MD_COMPLETE,80,SIZE_MAX,2,COG_TUI_MD_FORMAT_PLAIN);
    flatten(f,out,sizeof out);assert(strcmp(out,"three\nfour")==0);assert(view(f).first_row==2 && view(f).total_rows==4);(void)cog_tui_md_frame_free(&f);
    f=render(md,"a\n\nb\n",5,COG_TUI_MD_COMPLETE,80,SIZE_MAX,2,COG_TUI_MD_FORMAT_PLAIN);flatten(f,out,sizeof out);assert(strcmp(out,"\nb")==0);(void)cog_tui_md_frame_free(&f);
    f=render(md,"A\xcc\x81中",6,COG_TUI_MD_COMPLETE,2,0,10,COG_TUI_MD_FORMAT_PLAIN);assert(view(f).row_count==2);assert(view(f).rows[0].runs[0].bytes==3);(void)cog_tui_md_frame_free(&f);
    f=render(md,"\xf0\x9f",2,COG_TUI_MD_STREAMING,20,0,10,COG_TUI_MD_FORMAT_MARKDOWN);assert(view(f).pending_utf8_bytes==2);(void)cog_tui_md_frame_free(&f);
    f=render(md,"\xf0\x9f",2,COG_TUI_MD_FAILED,20,0,10,COG_TUI_MD_FORMAT_MARKDOWN);flatten(f,out,sizeof out);assert(strcmp(out,"\\xF0\\x9F")==0);(void)cog_tui_md_frame_free(&f);
    f=render(md,"A\0B",3,COG_TUI_MD_COMPLETE,80,0,10,COG_TUI_MD_FORMAT_PLAIN);flatten(f,out,sizeof out);assert(strcmp(out,"A\\x00B")==0);(void)cog_tui_md_frame_free(&f);
    f=render(md,"A\0B",3,COG_TUI_MD_COMPLETE,80,0,10,COG_TUI_MD_FORMAT_MARKDOWN);flatten(f,out,sizeof out);assert(strcmp(out,"A\\x00B")==0);(void)cog_tui_md_frame_free(&f);
    const char interior_utf8[]="|h|b|\n|-|-|\n|A\xf0\x9f|B|";
    f=render(md,interior_utf8,sizeof interior_utf8-1,COG_TUI_MD_STREAMING,40,0,20,COG_TUI_MD_FORMAT_MARKDOWN);
    flatten(f,out,sizeof out);assert(strstr(out,"A\\xF0\\x9F")!=NULL && view(f).pending_utf8_bytes==0);(void)cog_tui_md_frame_free(&f);
    const char overflow[]="|a|b|\n|-|-|\n|x|y|EXTRA|";cog_tui_md_source_t source={overflow,sizeof overflow-1,1,COG_TUI_MD_COMPLETE};cog_tui_md_window_t window={80,0,10};
    assert(cog_tui_markdown_render(md,&source,COG_TUI_MD_FORMAT_MARKDOWN,&window,&f)==COG_ERROR_PROTOCOL && f==NULL);
    assert(cog_tui_markdown_render(md,&source,COG_TUI_MD_FORMAT_PLAIN,&window,&f)==COG_OK);flatten(f,out,sizeof out);assert(strstr(out,"EXTRA")!=NULL);(void)cog_tui_md_frame_free(&f);
    cog_tui_md_limits_t limits={.source_bytes=2,.rows=2};cog_tui_markdown_t *small=NULL;assert(cog_tui_markdown_new(&small,&limits)==COG_OK);
    assert(cog_tui_markdown_render(small,&source,COG_TUI_MD_FORMAT_MARKDOWN,&window,&f)==COG_ERROR_LIMIT);
    assert(cog_tui_markdown_render(small,&source,COG_TUI_MD_FORMAT_PLAIN,&window,&f)==COG_OK);(void)cog_tui_md_frame_free(&f);(void)cog_tui_markdown_free(&small);
    limits=(cog_tui_md_limits_t){.frame_bytes=1};assert(cog_tui_markdown_new(&small,&limits)==COG_OK);assert(cog_tui_markdown_render(small,&source,COG_TUI_MD_FORMAT_PLAIN,&window,&f)==COG_ERROR_LIMIT);(void)cog_tui_markdown_free(&small);
    limits=(cog_tui_md_limits_t){.nesting=1};assert(cog_tui_markdown_new(&small,&limits)==COG_OK);
    source=(cog_tui_md_source_t){"> text",6,1,COG_TUI_MD_COMPLETE};assert(cog_tui_markdown_render(small,&source,COG_TUI_MD_FORMAT_MARKDOWN,&window,&f)==COG_ERROR_LIMIT);(void)cog_tui_markdown_free(&small);
    limits=(cog_tui_md_limits_t){.rows=1};assert(cog_tui_markdown_new(&small,&limits)==COG_OK);
    source=(cog_tui_md_source_t){"a\n\nb",4,1,COG_TUI_MD_COMPLETE};assert(cog_tui_markdown_render(small,&source,COG_TUI_MD_FORMAT_MARKDOWN,&window,&f)==COG_ERROR_LIMIT);(void)cog_tui_markdown_free(&small);
    limits=(cog_tui_md_limits_t){.table_columns=1};assert(cog_tui_markdown_new(&small,&limits)==COG_OK);
    source=(cog_tui_md_source_t){overflow,sizeof overflow-1,1,COG_TUI_MD_COMPLETE};assert(cog_tui_markdown_render(small,&source,COG_TUI_MD_FORMAT_MARKDOWN,&window,&f)==COG_ERROR_LIMIT);(void)cog_tui_markdown_free(&small);
    limits=(cog_tui_md_limits_t){.nesting=SIZE_MAX};assert(cog_tui_markdown_new(&small,&limits)==COG_ERROR_INVALID_ARG && small==NULL);
    assert(cog_tui_markdown_new(NULL,NULL)==COG_ERROR_NULL_PTR);assert(cog_tui_markdown_render(md,NULL,COG_TUI_MD_FORMAT_PLAIN,&window,&f)==COG_ERROR_NULL_PTR);
    source.message_key=0;assert(cog_tui_markdown_render(md,&source,COG_TUI_MD_FORMAT_PLAIN,&window,&f)==COG_ERROR_INVALID_ARG);source.message_key=1;window.columns=0;assert(cog_tui_markdown_render(md,&source,COG_TUI_MD_FORMAT_PLAIN,&window,&f)==COG_ERROR_INVALID_ARG);
    size_t unchanged=77;assert(cog_tui_md_frame_storage(NULL,&unchanged)==COG_ERROR_NULL_PTR && unchanged==77);
    assert(cog_tui_md_frame_view(NULL,NULL)==COG_ERROR_NULL_PTR);assert(cog_tui_md_frame_free(NULL)==COG_ERROR_NULL_PTR);assert(cog_tui_md_frame_free(&f)==COG_OK);
    f=render(md,"owned",5,COG_TUI_MD_COMPLETE,80,0,10,COG_TUI_MD_FORMAT_MARKDOWN);assert(cog_tui_markdown_free(&md)==COG_OK);flatten(f,out,sizeof out);assert(strcmp(out,"owned")==0);(void)cog_tui_md_frame_free(&f);
    puts("Markdown contract/prefix/viewport/source-fidelity matrix passed");return 0;
}
