#define _XOPEN_SOURCE 700
#include <cog_tui/markdown.h>
#include "md4c.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

struct cog_tui_markdown { cog_tui_md_limits_t limits; };
struct cog_tui_md_frame {
    cog_tui_md_view_t view;
    cog_tui_md_row_t *rows;
    size_t capacity, used;
};
typedef struct {
    cog_tui_markdown_t *owner;
    cog_tui_md_frame_t *frame;
    cog_tui_md_window_t window;
    cog_tui_md_format_t format;
    cog_result_t error;
    size_t row, column, depth, quote, lists;
    unsigned style, saved[128];
    size_t spans;
    unsigned ordinal[128];
    bool ordered[128];
    bool code, table, header, cell, row_started;
    size_t columns, cell_index;
    char *cells[32], *headers[32];
    size_t lengths[32], header_lengths[32], table_bytes;
    const char *source;
    size_t source_size, max_pipe_cells;
    bool pipes_scanned;
} builder;

static void clear_row(cog_tui_md_frame_t *f, cog_tui_md_row_t *row)
{
    cog_tui_md_run_t *runs=(cog_tui_md_run_t *)row->runs;
    for(size_t i=0;i<row->run_count;i++) { f->used-=runs[i].bytes; free((void *)runs[i].text); }
    f->used-=row->run_count*sizeof *runs;
    free(runs); memset(row,0,sizeof *row);
}
static bool budget(builder *b,size_t n)
{
    if(n>b->owner->limits.frame_bytes || b->frame->used>b->owner->limits.frame_bytes-n ||
       b->table_bytes>b->owner->limits.frame_bytes-n-b->frame->used) {
        b->error=COG_ERROR_LIMIT;return false;
    }
    return true;
}
static cog_tui_md_row_t *current(builder *b)
{
    size_t i;
    if(b->window.first_row==SIZE_MAX) i=b->row%b->window.row_count;
    else {
        if(b->row<b->window.first_row || b->row-b->window.first_row>=b->window.row_count)return NULL;
        i=b->row-b->window.first_row;
    }
    return &b->frame->rows[i];
}
static bool newline(builder *b)
{
    if(b->error!=COG_OK)return false;
    if(b->row==SIZE_MAX-1) { b->error=COG_ERROR_LIMIT;return false; }
    if(!b->row_started) { cog_tui_md_row_t *empty=current(b); if(empty!=NULL)clear_row(b->frame,empty); }
    b->row++;b->column=0;b->row_started=false;
    if(b->format==COG_TUI_MD_FORMAT_MARKDOWN && b->row>b->owner->limits.rows) {
        b->error=COG_ERROR_LIMIT;return false;
    }
    return true;
}
static bool emit(builder *b,const char *s,size_t n,size_t cells,unsigned style,cog_tui_md_origin_t origin)
{
    if(b->error!=COG_OK)return false;
    if(b->format==COG_TUI_MD_FORMAT_MARKDOWN && b->row>=b->owner->limits.rows) { b->error=COG_ERROR_LIMIT;return false; }
    if(cells>b->window.columns) { s="?";n=1;cells=1; }
    if(cells>b->window.columns-b->column) {
        if(!newline(b))return false;
        if(b->code && b->window.columns>2 && cells<=b->window.columns-2) {
            if(!emit(b,"> ",2,2,COG_TUI_MD_DIM,COG_TUI_MD_DECORATION))return false;
        }
    }
    cog_tui_md_row_t *r=current(b);
    if(r!=NULL) {
        if(!b->row_started)clear_row(b->frame,r);
        cog_tui_md_run_t *runs=(cog_tui_md_run_t *)r->runs;
        cog_tui_md_run_t *run=NULL;
        if(r->run_count>0 && runs[r->run_count-1].style==style && runs[r->run_count-1].origin==origin)
            run=&runs[r->run_count-1];
        if(run==NULL) {
            if(!budget(b,sizeof *runs))return false;
            if(r->run_count==SIZE_MAX/sizeof *runs) { b->error=COG_ERROR_LIMIT;return false; }
            runs=realloc(runs,(r->run_count+1)*sizeof *runs);
            if(runs==NULL) { b->error=COG_ERROR_NOMEM;return false; }
            r->runs=runs;run=&runs[r->run_count++];memset(run,0,sizeof *run);
            run->column=b->column;run->style=style;run->origin=origin;b->frame->used+=sizeof *runs;
        }
        if(!budget(b,n))return false;
        char *text=realloc((void *)run->text,run->bytes+n);
        if(text==NULL && n!=0) { b->error=COG_ERROR_NOMEM;return false; }
        memcpy(text+run->bytes,s,n);run->text=text;run->bytes+=n;run->cells+=cells;b->frame->used+=n;
        r->provisional=b->frame->view.phase==COG_TUI_MD_STREAMING;
    }
    b->row_started=true;b->column+=cells;
    if(origin==COG_TUI_MD_PREVIEW)b->frame->view.preview_applied=true;
    return true;
}
static bool ascii(builder *b,const char *s,unsigned style,cog_tui_md_origin_t origin)
{
    while(*s!='\0') { if(!emit(b,s,1,1,style,origin))return false;s++; }return true;
}
/* Strict UTF-8; -1 invalid, 0 potentially valid incomplete suffix. */
static int decode(const unsigned char *s,size_t n,uint32_t *cp)
{
    if(n==0)return 0;
    unsigned k;uint32_t v;
    if(s[0]<0x80) { *cp=s[0];return 1; }
    if(s[0]>=0xc2 && s[0]<=0xdf) { k=2;v=s[0]&31u; }
    else if(s[0]>=0xe0 && s[0]<=0xef) { k=3;v=s[0]&15u; }
    else if(s[0]>=0xf0 && s[0]<=0xf4) { k=4;v=s[0]&7u; }
    else return -1;
    for(unsigned i=1;i<k;i++) {
        if(i>=n)return 0;
        if((s[i]&0xc0u)!=0x80u)return -1;
        if(i==1 && ((s[0]==0xe0 && s[i]<0xa0) || (s[0]==0xed && s[i]>=0xa0) ||
                    (s[0]==0xf0 && s[i]<0x90) || (s[0]==0xf4 && s[i]>=0x90)))return -1;
        v=(v<<6)|(s[i]&63u);
    }
    *cp=v;return (int)k;
}
static bool unsupported(uint32_t cp)
{
    return (cp>=0x7f && cp<=0x9f) || cp==0xad || cp==0x61c || cp==0x180e ||
        (cp>=0x200b && cp<=0x200f) || (cp>=0x2028 && cp<=0x202e) ||
        (cp>=0x2060 && cp<=0x206f) || (cp>=0xfe00 && cp<=0xfe0f) || cp==0xfeff ||
        (cp>=0x1f000 && cp<=0x1faff) || (cp>=0xe0000 && cp<=0xe01ff);
}
static bool text(builder *b,const char *s,size_t n,unsigned style,cog_tui_md_origin_t origin)
{
    for(size_t i=0;i<n;) {
        unsigned char c=(unsigned char)s[i];
        if(c=='\n') { if(!newline(b))return false;i++;continue; }
        if(c=='\t') { size_t spaces=4-b->column%4;while(spaces-->0)if(!emit(b," ",1,1,style,origin))return false;i++;continue; }
        uint32_t cp=0;int k=decode((const unsigned char *)s+i,n-i,&cp);
        int width=k>0 && cp<=INT_MAX ? wcwidth((wchar_t)cp):-1;
        if(k<=0 || cp<32 || unsupported(cp) || width<0 || width==0) {
            char escaped[16];int count;
            if(k<=0 || cp<128) { count=snprintf(escaped,sizeof escaped,"\\x%02X",c);k=1; }
            else count=snprintf(escaped,sizeof escaped,"\\u%04X",(unsigned)cp);
            if(count<0 || !ascii(b,escaped,style,origin))return false;
            i+=(size_t)k;continue;
        }
        size_t cluster=(size_t)k;
        while(i+cluster<n) {
            uint32_t mark;int m=decode((const unsigned char *)s+i+cluster,n-i-cluster,&mark);
            if(m<=0 || mark<32 || unsupported(mark) || mark>INT_MAX || wcwidth((wchar_t)mark)!=0)break;
            cluster+=(size_t)m;
        }
        if((size_t)width>b->window.columns) {
            char escaped[16];(void)snprintf(escaped,sizeof escaped,"\\u%04X",(unsigned)cp);
            if(!ascii(b,escaped,style,origin))return false;
            i+=(size_t)k;continue;
        }
        if(!emit(b,s+i,cluster,(size_t)width,style,origin))return false;
        i+=cluster;
    }
    return true;
}
static bool line_start(builder *b)
{
    return b->column==0 || newline(b);
}
static bool border(builder *b,cog_tui_md_origin_t origin)
{
    if(!line_start(b))return false;
    size_t width=b->window.columns<48?b->window.columns:48;
    for(size_t i=0;i<width;i++)if(!emit(b,"-",1,1,COG_TUI_MD_DIM,origin))return false;
    return newline(b);
}
static bool prefix(builder *b)
{
    if(!line_start(b))return false;
    for(size_t i=0;i<b->quote;i++)if(!ascii(b,"> ",COG_TUI_MD_DIM,COG_TUI_MD_DECORATION))return false;
    for(size_t i=1;i<b->lists;i++)if(!ascii(b,"  ",0,COG_TUI_MD_DECORATION))return false;
    return true;
}
static bool table_append(builder *b,const char *s,size_t n)
{
    size_t i=b->cell_index;
    if(i>=b->columns) { b->error=COG_ERROR_PROTOCOL;return false; }
    if(n>SIZE_MAX-b->lengths[i]-1 || !budget(b,n+1))return false;
    char *p=realloc(b->cells[i],b->lengths[i]+n+1);
    if(p==NULL) { b->error=COG_ERROR_NOMEM;return false; }
    if(b->cells[i]==NULL)b->table_bytes++;
    memcpy(p+b->lengths[i],s,n);b->lengths[i]+=n;p[b->lengths[i]]='\0';b->cells[i]=p;b->table_bytes+=n;return true;
}
static void table_clear(builder *b)
{
    for(size_t i=0;i<32;i++) { if(b->cells[i])b->table_bytes-=b->lengths[i]+1;free(b->cells[i]);b->cells[i]=NULL;b->lengths[i]=0; }
}
static bool table_row(builder *b)
{
    if(!line_start(b))return false;
    unsigned style=b->header?COG_TUI_MD_BOLD:0;
    bool vertical=b->window.columns<4*b->columns+1;
    if(vertical) {
        for(size_t i=0;i<b->columns;i++) {
            const char *label=b->headers[i];
            if(!b->header && label!=NULL) { if(!text(b,label,b->header_lengths[i],COG_TUI_MD_BOLD,COG_TUI_MD_SOURCE) || !ascii(b,": ",0,COG_TUI_MD_DECORATION))return false; }
            if(!text(b,b->cells[i]?b->cells[i]:"",b->lengths[i],style,COG_TUI_MD_SOURCE) || !newline(b))return false;
        }
    } else {
        size_t width=(b->window.columns-b->columns-1)/b->columns;
        cog_tui_md_frame_t *frames[32]={0};cog_tui_md_view_t views[32];bool accounted[32]={false};size_t lines=1;
        for(size_t i=0;i<b->columns;i++) {
            /* Only the authoritative source suffix can be pending UTF-8. A cell
             * boundary is not a provider-stream boundary. */
            cog_tui_md_source_t source={b->cells[i],b->lengths[i],1,COG_TUI_MD_COMPLETE};
            cog_tui_md_window_t window={width,0,300};
            b->error=cog_tui_markdown_render(b->owner,&source,COG_TUI_MD_FORMAT_PLAIN,&window,&frames[i]);
            if(b->error!=COG_OK)break;
            (void)cog_tui_md_frame_view(frames[i],&views[i]);
            if(views[i].total_rows>300 || !budget(b,frames[i]->used)) { b->error=COG_ERROR_LIMIT;break; }
            b->table_bytes+=frames[i]->used;accounted[i]=true;
            if(views[i].row_count>lines)lines=views[i].row_count;
        }
        for(size_t line=0;line<lines && b->error==COG_OK;line++) {
            if(!ascii(b,"|",COG_TUI_MD_DIM,COG_TUI_MD_DECORATION))break;
            for(size_t i=0;i<b->columns;i++) {
                size_t cells=0;
                if(line<views[i].row_count)for(size_t r=0;r<views[i].rows[line].run_count;r++) {
                    const cog_tui_md_run_t *run=&views[i].rows[line].runs[r];
                    if(!emit(b,run->text,run->bytes,run->cells,style,COG_TUI_MD_SOURCE))break;
                    cells+=run->cells;
                }
                while(cells++<width)if(!emit(b," ",1,1,0,COG_TUI_MD_DECORATION))break;
                if(!ascii(b,"|",COG_TUI_MD_DIM,COG_TUI_MD_DECORATION))break;
            }
            if(!newline(b))break;
        }
        for(size_t i=0;i<b->columns;i++)if(frames[i]) { /* Account only frames added above. */
            if(accounted[i])b->table_bytes-=frames[i]->used;
            (void)cog_tui_md_frame_free(&frames[i]);
        }
        if(b->error!=COG_OK)return false;
    }
    if(b->header)for(size_t i=0;i<b->columns;i++) {
        b->headers[i]=b->cells[i];b->header_lengths[i]=b->lengths[i];b->cells[i]=NULL;b->lengths[i]=0;
    }
    table_clear(b);return true;
}
/* GFM splits unescaped pipes even within inline code. A conservative preflight
 * against every pipe-containing source line prevents silently discarded cells.
 * It can choose raw fallback for unrelated prose with many pipes. */
static bool table_overflow(builder *b,size_t columns)
{
    /* A document can contain thousands of tables. Scan source once rather than
     * once per table, preserving the conservative whole-document policy. */
    if(!b->pipes_scanned) {
        size_t start=0;
        while(start<b->source_size) {
            size_t end=start;while(end<b->source_size && b->source[end]!='\n')end++;
            size_t a=start,z=end;while(a<z && (b->source[a]==' ' || b->source[a]=='\t'))a++;
            while(z>a && (b->source[z-1]==' ' || b->source[z-1]=='\t' || b->source[z-1]=='\r'))z--;
            size_t pipes=0,last=SIZE_MAX;bool escaped=false;
            for(size_t i=a;i<z;i++) { if(b->source[i]=='|' && !escaped) { pipes++;last=i; }if(b->source[i]=='\\')escaped=!escaped;else escaped=false; }
            if(pipes>0) {
                size_t count=pipes+1;if(a<z && b->source[a]=='|')count--;if(last==z-1)count--;
                if(count>b->max_pipe_cells)b->max_pipe_cells=count;
            }
            start=end<b->source_size?end+1:end;
        }
        b->pipes_scanned=true;
    }
    return b->max_pipe_cells>columns;
}

static int enter_block(MD_BLOCKTYPE type,void *detail,void *userdata)
{
    builder *b=userdata;
    if(++b->depth>b->owner->limits.nesting) { b->error=COG_ERROR_LIMIT;return 1; }
    switch(type) {
    case MD_BLOCK_QUOTE:b->quote++;break;
    case MD_BLOCK_UL:case MD_BLOCK_OL:
        if(b->lists>=128) { b->error=COG_ERROR_LIMIT;return 1; }
        b->ordered[b->lists]=type==MD_BLOCK_OL;
        b->ordinal[b->lists]=type==MD_BLOCK_OL?((MD_BLOCK_OL_DETAIL *)detail)->start:0;
        b->lists++;break;
    case MD_BLOCK_LI: {
        char bullet[32]="* ";
        if(b->lists>0 && b->ordered[b->lists-1]) {
            (void)snprintf(bullet,sizeof bullet,"%u. ",b->ordinal[b->lists-1]);
            if(b->ordinal[b->lists-1]<UINT_MAX)b->ordinal[b->lists-1]++;
        }
        if(!prefix(b) || !ascii(b,bullet,0,COG_TUI_MD_DECORATION))return 1;
        break;
    }
    case MD_BLOCK_H:b->style|=COG_TUI_MD_BOLD;if(!prefix(b))return 1;
        break;
    case MD_BLOCK_P:if(b->column==0 && !prefix(b))return 1;
        break;
    case MD_BLOCK_HR:if(!border(b,COG_TUI_MD_DECORATION))return 1;
        break;
    case MD_BLOCK_CODE: {
        MD_BLOCK_CODE_DETAIL *d=detail;
        if(!border(b,COG_TUI_MD_DECORATION))return 1;
        if(d->lang.size && (!text(b,d->lang.text,d->lang.size,COG_TUI_MD_DIM,COG_TUI_MD_SOURCE) || !newline(b)))return 1;
        b->code=true;b->style|=COG_TUI_MD_CODE;break;
    }
    case MD_BLOCK_TABLE: {
        MD_BLOCK_TABLE_DETAIL *d=detail;b->columns=d->col_count;
        if(b->columns==0 || b->columns>b->owner->limits.table_columns || b->columns>32) { b->error=COG_ERROR_LIMIT;return 1; }
        if(table_overflow(b,b->columns)) { b->error=COG_ERROR_PROTOCOL;return 1; }
        b->table=true;if(!border(b,COG_TUI_MD_DECORATION))return 1;
        break;
    }
    case MD_BLOCK_TR:b->cell_index=0;break;
    case MD_BLOCK_TH:b->header=true;b->cell=true;break;
    case MD_BLOCK_TD:b->header=false;b->cell=true;break;
    default:break;
    }
    return b->error!=COG_OK;
}
static int leave_block(MD_BLOCKTYPE type,void *detail,void *userdata)
{
    builder *b=userdata;(void)detail;
    switch(type) {
    case MD_BLOCK_QUOTE:b->quote--;break;
    case MD_BLOCK_UL:case MD_BLOCK_OL:b->lists--;break;
    case MD_BLOCK_H:b->style&=~(unsigned)COG_TUI_MD_BOLD;/* fall through */
    case MD_BLOCK_P:case MD_BLOCK_LI:if(!line_start(b))return 1;
        break;
    case MD_BLOCK_CODE:
        b->code=false;b->style&=~(unsigned)COG_TUI_MD_CODE;
        if(!border(b,b->frame->view.phase==COG_TUI_MD_STREAMING?COG_TUI_MD_PREVIEW:COG_TUI_MD_DECORATION))return 1;
        break;
    case MD_BLOCK_TH:case MD_BLOCK_TD:b->cell=false;b->cell_index++;break;
    case MD_BLOCK_TR:if(!table_row(b))return 1;
        break;
    case MD_BLOCK_TABLE:
        b->table=false;
        if(!border(b,b->frame->view.phase==COG_TUI_MD_STREAMING?COG_TUI_MD_PREVIEW:COG_TUI_MD_DECORATION))return 1;
        for(size_t i=0;i<32;i++)if(b->headers[i]) { b->table_bytes-=b->header_lengths[i]+1;free(b->headers[i]);b->headers[i]=NULL; }
        break;
    default:break;
    }
    b->depth--;return b->error!=COG_OK;
}
static int enter_span(MD_SPANTYPE type,void *detail,void *userdata)
{
    builder *b=userdata;(void)detail;
    if(b->spans>=128 || b->depth+b->spans>=b->owner->limits.nesting) { b->error=COG_ERROR_LIMIT;return 1; }
    b->saved[b->spans++]=b->style;
    if(type==MD_SPAN_STRONG)b->style|=COG_TUI_MD_BOLD;
    if(type==MD_SPAN_EM)b->style|=COG_TUI_MD_ITALIC;
    if(type==MD_SPAN_CODE)b->style|=COG_TUI_MD_CODE;
    if(type==MD_SPAN_A)b->style|=COG_TUI_MD_UNDERLINE;
    if(type==MD_SPAN_IMG && !(b->cell?table_append(b,"[image: ",8):ascii(b,"[image: ",COG_TUI_MD_DIM,COG_TUI_MD_DECORATION)))return 1;
    return 0;
}
static int leave_span(MD_SPANTYPE type,void *detail,void *userdata)
{
    builder *b=userdata;
    if(type==MD_SPAN_A) {
        MD_SPAN_A_DETAIL *d=detail;
        if(b->cell) { if(!table_append(b," (",2) || !table_append(b,d->href.text,d->href.size) || !table_append(b,")",1))return 1; }
        else if(!ascii(b," (",COG_TUI_MD_DIM,COG_TUI_MD_DECORATION) || !text(b,d->href.text,d->href.size,COG_TUI_MD_DIM,COG_TUI_MD_SOURCE) || !ascii(b,")",COG_TUI_MD_DIM,COG_TUI_MD_DECORATION))return 1;
    }
    if(type==MD_SPAN_IMG && !(b->cell?table_append(b,"]",1):ascii(b,"]",COG_TUI_MD_DIM,COG_TUI_MD_DECORATION)))return 1;
    if(b->spans==0) { b->error=COG_ERROR_PROTOCOL;return 1; }
    b->style=b->saved[--b->spans];return 0;
}
static int text_callback(MD_TEXTTYPE type,const MD_CHAR *s,MD_SIZE n,void *userdata)
{
    builder *b=userdata;
    if(type==MD_TEXT_SOFTBR) { s=" ";n=1; }
    if(type==MD_TEXT_BR) { s="\n";n=1; }
    if(type==MD_TEXT_NULLCHAR) { s="\\x00";n=4; }
    return !(b->cell?table_append(b,s,n):text(b,s,n,b->style,COG_TUI_MD_SOURCE));
}

cog_result_t cog_tui_markdown_new(cog_tui_markdown_t **out,const cog_tui_md_limits_t *limits)
{
    if(out==NULL)return COG_ERROR_NULL_PTR;
    *out=NULL;
    cog_tui_md_limits_t l=limits?*limits:(cog_tui_md_limits_t){0};
    if(!l.source_bytes)l.source_bytes=131072;
    if(!l.frame_bytes)l.frame_bytes=4194304;
    if(!l.rows)l.rows=16384;
    if(!l.nesting)l.nesting=64;
    if(!l.table_columns)l.table_columns=32;
    if(l.source_bytes>UINT_MAX || l.frame_bytes>SIZE_MAX/2 || l.rows==SIZE_MAX || l.nesting>128 || l.table_columns>32)return COG_ERROR_INVALID_ARG;
    cog_tui_markdown_t *m=malloc(sizeof *m);if(m==NULL)return COG_ERROR_NOMEM;m->limits=l;*out=m;return COG_OK;
}
cog_result_t cog_tui_markdown_render(cog_tui_markdown_t *markdown,const cog_tui_md_source_t *source,cog_tui_md_format_t format,const cog_tui_md_window_t *window,cog_tui_md_frame_t **out)
{
    if(out==NULL)return COG_ERROR_NULL_PTR;
    *out=NULL;
    if(markdown==NULL || source==NULL || window==NULL)return COG_ERROR_NULL_PTR;
    if((source->bytes==NULL && source->byte_count!=0) || !source->message_key || source->phase<COG_TUI_MD_STREAMING || source->phase>COG_TUI_MD_FAILED ||
       format<COG_TUI_MD_FORMAT_MARKDOWN || format>COG_TUI_MD_FORMAT_PLAIN || window->columns<1 || window->columns>1000 || window->row_count<1 || window->row_count>300)return COG_ERROR_INVALID_ARG;
    if(format==COG_TUI_MD_FORMAT_MARKDOWN && source->byte_count>markdown->limits.source_bytes)return COG_ERROR_LIMIT;
    size_t base=sizeof(cog_tui_md_frame_t)+window->row_count*sizeof(cog_tui_md_row_t);
    if(base>markdown->limits.frame_bytes)return COG_ERROR_LIMIT;
    cog_tui_md_frame_t *f=calloc(1,sizeof *f);if(f==NULL)return COG_ERROR_NOMEM;
    f->rows=calloc(window->row_count,sizeof *f->rows);if(f->rows==NULL) { free(f);return COG_ERROR_NOMEM; }
    f->used=base;f->capacity=window->row_count;f->view.columns=window->columns;f->view.message_key=source->message_key;f->view.phase=source->phase;
    size_t n=source->byte_count;
    if(source->phase==COG_TUI_MD_STREAMING) {
        size_t start=n>3?n-3:0;
        for(size_t i=start;i<n;i++) { uint32_t cp;if(decode((const unsigned char *)source->bytes+i,n-i,&cp)==0) { f->view.pending_utf8_bytes=n-i;n=i;break; } }
    }
    builder b={0};b.owner=markdown;b.frame=f;b.window=*window;b.format=format;b.source=source->bytes;b.source_size=n;
    if(format==COG_TUI_MD_FORMAT_PLAIN)(void)text(&b,source->bytes,n,0,COG_TUI_MD_SOURCE);
    else {
        MD_PARSER parser={0};parser.flags=MD_FLAG_TABLES|MD_FLAG_NOHTMLBLOCKS|MD_FLAG_NOHTMLSPANS;
        parser.enter_block=enter_block;parser.leave_block=leave_block;parser.enter_span=enter_span;parser.leave_span=leave_span;parser.text=text_callback;
        if(md_parse(source->bytes?source->bytes:"",(MD_SIZE)n,&parser,&b)!=0 && b.error==COG_OK)b.error=COG_ERROR_PROTOCOL;
    }
    table_clear(&b);for(size_t i=0;i<32;i++)free(b.headers[i]);
    if(b.error!=COG_OK) { (void)cog_tui_md_frame_free(&f);return b.error; }
    size_t total=b.row+(b.column>0 || b.row==0?1u:0u);
    /* Drop only the empty layout row after a trailing newline, not authored rows. */
    f->view.total_rows=total;
    size_t first=window->first_row==SIZE_MAX?(total>window->row_count?total-window->row_count:0):window->first_row;
    size_t count=first<total?total-first:0;if(count>window->row_count)count=window->row_count;
    if(window->first_row==SIZE_MAX && total>window->row_count) {
        size_t shift=first%window->row_count;
        /* Rotate ownership in place; no second metadata allocation. */
        for(size_t j=0;j<shift;j++) {
            cog_tui_md_row_t temp=f->rows[0];
            memmove(f->rows,f->rows+1,(window->row_count-1)*sizeof *f->rows);
            f->rows[window->row_count-1]=temp;
        }
    }
    f->view.first_row=first;f->view.row_count=count;f->view.rows=f->rows;*out=f;return COG_OK;
}
cog_result_t cog_tui_md_frame_view(const cog_tui_md_frame_t *frame,cog_tui_md_view_t *view)
{
    if(frame==NULL || view==NULL)return COG_ERROR_NULL_PTR;
    *view=frame->view;return COG_OK;
}
cog_result_t cog_tui_md_frame_storage(const cog_tui_md_frame_t *frame,size_t *bytes)
{
    if(frame==NULL || bytes==NULL)return COG_ERROR_NULL_PTR;
    *bytes=frame->used;return COG_OK;
}
cog_result_t cog_tui_md_frame_free(cog_tui_md_frame_t **frame)
{
    if(frame==NULL)return COG_ERROR_NULL_PTR;
    if(*frame==NULL)return COG_OK;
    for(size_t i=0;i<(*frame)->capacity;i++)clear_row(*frame,&(*frame)->rows[i]);
    free((*frame)->rows);free(*frame);*frame=NULL;return COG_OK;
}
cog_result_t cog_tui_markdown_free(cog_tui_markdown_t **markdown)
{
    if(markdown==NULL)return COG_ERROR_NULL_PTR;
    free(*markdown);*markdown=NULL;return COG_OK;
}
