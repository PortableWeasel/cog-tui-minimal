#include <cog_cli/cli.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { cog_result_t r=(x); if(r!=COG_OK){fprintf(stderr,"line %d: %d\n",__LINE__,r);return 1;} } while(0)
int main(void) {
 cog_cli_options_t opts={0}; cog_cli_app_t *app=NULL;
 char *args[]={"probe","--mock","--no-store"};
 int exit_code=0; CHECK(cog_cli_options_parse(&opts,3,args,&exit_code));
 CHECK(cog_cli_app_new(&app,&opts));
 cog_core_t *core=cog_cli_app_core(app);
 CHECK(cog_core_drive_begin(core));
 CHECK(cog_cli_app_submit(app,"hello",5));
 cog_core_step_info_t step={0};
 int seen=0;
 for(unsigned i=0;i<10000;i++) {
  CHECK(cog_core_step(core,&step));
  cog_cli_display_snapshot_t view={0};
  CHECK(cog_cli_app_display_snapshot(app,&view));
  for(size_t n=0;n<view.item_count;n++) if(view.items[n].role==COG_MESSAGE_ROLE_ASSISTANT && view.items[n].text_len) seen=1;
  if(seen) break;
  if(step.done) break;
 }
 CHECK(cog_cli_app_shutdown(app));
 while(!step.done) CHECK(cog_core_step(core,&step));
 CHECK(cog_core_drive_end(core));
 cog_cli_app_free(app);
 fprintf(stderr,"snapshot assistant observed: %d\n",seen);
 return seen ? 0:1;
}
