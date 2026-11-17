#define _POSIX_C_SOURCE 200809L
#include <cog_tui/settings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"settings:%d: %s\n",__LINE__,#x); return 1; } } while(0)
static int write_text(const char *path, const char *text)
{
    FILE *f=fopen(path,"w"); if(!f) return -1;
    int ok=fputs(text,f)>=0; if(fclose(f))ok=0;
    return ok ? 0:-1;
}
int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    char dir[4096],path[4096],linkpath[4096],badpath[4096];
    CHECK(snprintf(dir,sizeof(dir),"%s/settings-test-XXXXXX",argv[1])>0);
    CHECK(mkdtemp(dir)!=NULL);
    CHECK(snprintf(path,sizeof(path),"%s/config",dir)>0);
    CHECK(snprintf(linkpath,sizeof(linkpath),"%s/link",dir)>0);
    CHECK(snprintf(badpath,sizeof(badpath),"%s/missing/config",dir)>0);
    cog_tui_settings_t s,loaded,before;
    CHECK(cog_tui_settings_defaults(NULL)==COG_ERROR_NULL_PTR);
    CHECK(cog_tui_settings_defaults(&s)==COG_OK);
    CHECK(cog_tui_settings_validate(&s)==COG_ERROR_INVALID_ARG);
    strcpy(s.model,"fixture/model");
    CHECK(cog_tui_settings_validate(&s)==COG_OK);
    mode_t oldmask=umask(0777);
    CHECK(cog_tui_settings_save(path,&s)==COG_OK);
    (void)umask(oldmask);
    struct stat st;CHECK(stat(path,&st)==0 && (st.st_mode & 0777)==0600);
    CHECK(cog_tui_settings_load(path,&loaded)==COG_OK && !memcmp(&s,&loaded,sizeof(s)));
    before=loaded;
    CHECK(cog_tui_settings_load(badpath,&loaded)==COG_ERROR_NOT_FOUND && !memcmp(&before,&loaded,sizeof(before)));
    CHECK(cog_tui_settings_save(badpath,&s)==COG_ERROR_IO);
    CHECK(symlink(path,linkpath)==0);
    CHECK(cog_tui_settings_load(linkpath,&loaded)==COG_ERROR_DENIED);
    CHECK(cog_tui_settings_save(linkpath,&s)==COG_ERROR_DENIED);
    CHECK(chmod(path,0644)==0);
    CHECK(cog_tui_settings_load(path,&loaded)==COG_ERROR_DENIED);
    CHECK(cog_tui_settings_save(path,&s)==COG_ERROR_DENIED);
    CHECK(chmod(path,0600)==0);
    CHECK(chmod(dir,0755)==0);
    CHECK(cog_tui_settings_save(path,&s)==COG_ERROR_DENIED);
    CHECK(chmod(dir,0700)==0);
    char oldmodel[sizeof(s.model)];memcpy(oldmodel,s.model,sizeof(oldmodel));
    memset(s.model,'x',sizeof(s.model));
    CHECK(cog_tui_settings_validate(&s)==COG_ERROR_INVALID_ARG);
    CHECK(cog_tui_settings_save(path,&s)==COG_ERROR_INVALID_ARG);
    CHECK(cog_tui_settings_load(path,&loaded)==COG_OK && !strcmp(loaded.model,"fixture/model"));
    memcpy(s.model,oldmodel,sizeof(s.model));
    static const char *const refs[]={"raw-secret","env:","env:9KEY","env:A-B","env:A B","env:A\nB"};
    for(size_t i=0;i<sizeof(refs)/sizeof(refs[0]);++i) {
        strcpy(s.credential_ref,refs[i]);CHECK(cog_tui_settings_validate(&s)==COG_ERROR_INVALID_ARG);
    }
    strcpy(s.credential_ref,"env:KEY_2");
    static const char *const urls[]={"http://host","https://","https:///path","https://user@host","https://host?q=1","https://host#x","https://ho st"};
    for(size_t i=0;i<sizeof(urls)/sizeof(urls[0]);++i) {
        strcpy(s.base_url,urls[i]);CHECK(cog_tui_settings_validate(&s)==COG_ERROR_INVALID_ARG);
    }
    strcpy(s.base_url,"https://openrouter.ai/api/v1");
    strcpy(s.provider,"unknown");CHECK(cog_tui_settings_validate(&s)==COG_ERROR_UNSUPPORTED);
    strcpy(s.provider,"openrouter");
    static const char *const badfiles[]={"", "version=1\n", "version=1\nversion=1\n", "unknown=x\n", "version=2\n", "version=1\r\n"};
    for(size_t i=0;i<sizeof(badfiles)/sizeof(badfiles[0]);++i) {
        CHECK(write_text(path,badfiles[i])==0);before=loaded;
        CHECK(cog_tui_settings_load(path,&loaded)==COG_ERROR_PROTOCOL && !memcmp(&before,&loaded,sizeof(before)));
    }
    FILE *f=fopen(path,"w");CHECK(f!=NULL);
    for(size_t i=0;i<8193;++i)CHECK(fputc('x',f)!=EOF);
    CHECK(fclose(f)==0);CHECK(cog_tui_settings_load(path,&loaded)==COG_ERROR_LIMIT);
    CHECK(cog_tui_settings_save(path,&s)==COG_OK);
    CHECK(cog_tui_settings_load(path,&loaded)==COG_OK && !strcmp(loaded.credential_ref,"env:KEY_2"));
    CHECK(unlink(linkpath)==0);CHECK(unlink(path)==0);CHECK(rmdir(dir)==0);
    puts("settings contracts passed");return 0;
}
