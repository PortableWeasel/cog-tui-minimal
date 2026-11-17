#define _POSIX_C_SOURCE 200809L
#include <cog_tui/app.h>
#include <cog_tui/runtime.h>
#include <readline/readline.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

typedef struct app {
 FILE *output;
 cog_tui_settings_t settings, edit;
 cog_tui_runtime_t *runtime;
 cog_tui_runtime_options_t options;
 char settings_path[4096],config_path[4096];
 char *line;
 bool eof, quitting, busy, editing;
 unsigned field;
 size_t rendered[4096];
 bool finished[4096];
 uint64_t baseline;
} app_t;
/* Readline exposes no callback user-data. One bounded lifetime binding only. */
static app_t *binding;
static volatile sig_atomic_t interrupted;
static void on_signal(int signo){interrupted=signo;}
static void on_line(char *line){if(binding){binding->line=line;if(!line)binding->eof=true;}else free(line);}
static void say(app_t *a,const char *text){
 rl_save_prompt();rl_crlf();fputs(text,a->output);fputc('\n',a->output);
 rl_restore_prompt();rl_on_new_line();rl_redisplay();fflush(a->output);
}
static void error_message(app_t *a,const char *operation,cog_result_t rc){char msg[160];(void)snprintf(msg,sizeof(msg),"%s failed (Cog error %d). Use /settings to correct configuration.",operation,(int)rc);say(a,msg);}
static void escaped(FILE *out,const char *text,size_t n){for(size_t i=0;i<n;i++){unsigned char c=(unsigned char)text[i];if(c<32U&&c!='\n'&&c!='\t')fprintf(out,"\\x%02x",(unsigned)c);else if(c==127U)fputs("\\x7f",out);else fputc(c,out);}}
static void prompt(app_t *a){const char *p="cog> ";if(a->editing){static const char *const prompts[]={"Model: ","HTTPS base URL (blank keeps current): ","Credential reference env:NAME (blank keeps current): "};p=prompts[a->field];}rl_set_prompt(p);rl_on_new_line();rl_redisplay();}
static void start_runtime(app_t *a){
 cog_result_t rc=cog_tui_runtime_new(&a->runtime,&a->settings,&a->options);
 if(rc!=COG_OK){error_message(a,"Runtime setup",rc);say(a,"Check that the credential environment variable is set and the installed Cog dependency profile is compatible.");}
 else say(a,a->options.mock?"MOCK ready (offline fixture).":"Ready. Provider: openrouter. Network authentication is checked on first request.");
}
static void begin_settings(app_t *a){if(a->busy){say(a,"Finish or cancel the current turn before changing settings.");return;}a->edit=a->settings;a->editing=true;a->field=0;say(a,"Provider: openrouter. Enter a model ID; blank keeps the current value. Credentials use an existing environment variable, never a raw key.");prompt(a);}
static void accept_setting(app_t *a,const char *line){
 char *dest=a->field==0?a->edit.model:a->field==1?a->edit.base_url:a->edit.credential_ref;
 if(strlen(line)>=COG_TUI_VALUE_CAPACITY){say(a,"Value is too long.");return;}
 if(*line)strcpy(dest,line);
 if(a->field<2){a->field++;prompt(a);return;}
 cog_result_t rc=cog_tui_settings_validate(&a->edit);
 if(rc!=COG_OK){error_message(a,"Settings validation",rc);a->field=0;prompt(a);return;}
 rc=cog_tui_settings_save(a->settings_path,&a->edit);
 if(rc!=COG_OK){error_message(a,"Settings save",rc);return;}
 a->settings=a->edit;a->editing=false;
 say(a,"Settings saved.");
 if(a->runtime){rc=cog_tui_runtime_reload(a->runtime,&a->settings);if(rc!=COG_OK)error_message(a,"Apply saved settings",rc);else say(a,"Selection published for the next turn; current conversation retained.");}
 else start_runtime(a);
 prompt(a);
}
static void command_quit(app_t *a){a->quitting=true;}
static void command_cancel(app_t *a){if(!a->runtime||!a->busy){say(a,"No active turn.");return;}cog_result_t rc=cog_tui_runtime_cancel(a->runtime);if(rc!=COG_OK)error_message(a,"Cancel",rc);else say(a,"Cancellation requested; waiting for Cog outcome.");}
static void command_show(app_t *a){char msg[2300];(void)snprintf(msg,sizeof(msg),"Saved selection: provider=%s model=%s\nEndpoint=%s",a->settings.provider,a->settings.model,a->settings.base_url);say(a,msg);}
static void command_help(app_t *a){say(a,"/settings configure provider details and model; /show saved selection; /cancel active turn; /quit exit. Ctrl-C cancels a turn, or exits when idle. Tools are disabled in this version.");}
static void handle_line(app_t *a,const char *line){
 if(a->editing){accept_setting(a,line);return;}
 static const struct {const char *name;void (*run)(app_t *);} commands[]={{"/quit",command_quit},{"/cancel",command_cancel},{"/settings",begin_settings},{"/show",command_show},{"/help",command_help}};
 for(size_t i=0;i<sizeof(commands)/sizeof(commands[0]);i++)if(!strcmp(line,commands[i].name)){commands[i].run(a);return;}
 if(!*line)return;
 if(line[0]=='/'){say(a,"Unknown command. Use /help.");return;}
 if(!a->runtime){say(a,"Runtime unavailable. Use /settings to correct and retry setup.");return;}
 if(a->busy){say(a,"A turn is active. Wait or use /cancel; this prompt was not submitted.");return;}
 cog_cli_display_snapshot_t v; cog_result_t rc=cog_tui_runtime_snapshot(a->runtime,&v);
 if(rc!=COG_OK){error_message(a,"Snapshot",rc);return;}
 a->baseline=0;for(size_t i=0;i<v.item_count;i++)if(v.items[i].node_id>a->baseline)a->baseline=v.items[i].node_id;
 rc=cog_tui_runtime_submit(a->runtime,line,strlen(line));if(rc!=COG_OK)error_message(a,"Submit",rc);else a->busy=true;
}
static cog_result_t render(app_t *a){
 cog_cli_display_snapshot_t v;cog_result_t rc=cog_tui_runtime_snapshot(a->runtime,&v);if(rc!=COG_OK)return rc;
 if(v.item_count>4096)return COG_ERROR_LIMIT;
 for(size_t i=0;i<v.item_count;i++){
  const cog_cli_display_item_t *item=&v.items[i];if(item->role!=COG_MESSAGE_ROLE_ASSISTANT)continue;
  if(item->text_len>a->rendered[i]){
   rl_save_prompt();rl_crlf();fputs("assistant: ",a->output);escaped(a->output,item->text+a->rendered[i],item->text_len-a->rendered[i]);fputc('\n',a->output);rl_restore_prompt();rl_on_new_line();rl_redisplay();fflush(a->output);a->rendered[i]=item->text_len;
  }
  if(item->node_id&&!a->finished[i]){a->finished[i]=true;if(item->node_id>a->baseline)a->busy=false;
   if(item->status==COG_MESSAGE_CANCELLED)say(a,"Turn cancelled.");else if(item->status!=COG_MESSAGE_COMPLETE)say(a,"Turn incomplete or failed. Submit fresh input to continue; no automatic replay.");
  }
 }
 return COG_OK;
}
static int private_directory(const char *path){struct stat st;if(mkdir(path,0700)<0&&errno!=EEXIST)return -1;if(lstat(path,&st)<0||!S_ISDIR(st.st_mode)||st.st_uid!=geteuid()||(st.st_mode&077)!=0)return -1;return 0;}
int cog_tui_app_run(int argc,char **argv){
 if(argc<1||!argv)return 2;
 bool mock=false;const char *directory=NULL;
 for(int i=1;i<argc;i++){if(!strcmp(argv[i],"--help")){puts("Usage: cog-tui [--mock] [--config-dir DIRECTORY]\nInteractive readline client for installed Cog. --mock is an offline test fixture.");return 0;}else if(!strcmp(argv[i],"--mock"))mock=true;else if(!strcmp(argv[i],"--config-dir")&&i+1<argc)directory=argv[++i];else{fputs("Invalid option; use --help.\n",stderr);return 2;}}
 if(binding||!isatty(STDIN_FILENO)||!isatty(STDOUT_FILENO)){fputs("cog-tui requires an interactive terminal.\n",stderr);return 2;}
 app_t a={0};char dir[4096];
 if(!directory){const char *home=getenv("HOME");if(!home||snprintf(dir,sizeof(dir),"%s/.cog-tui",home)>=(int)sizeof(dir))return 2;directory=dir;}
 if(private_directory(directory)<0){fputs("Configuration directory must be private and owned by you.\n",stderr);return 2;}
 if(snprintf(a.settings_path,sizeof(a.settings_path),"%s/settings",directory)>=(int)sizeof(a.settings_path)||snprintf(a.config_path,sizeof(a.config_path),"%s/cog-empty.conf",directory)>=(int)sizeof(a.config_path))return 2;
 int config=open(a.config_path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
 if(config>=0)close(config);else if(errno!=EEXIST)return 2;
 struct stat st;if(lstat(a.config_path,&st)<0||!S_ISREG(st.st_mode)||st.st_uid!=geteuid()||(st.st_mode&077)!=0||st.st_size!=0){fputs("Unsafe or nonempty UI runtime config file.\n",stderr);return 2;}
 a.options=(cog_tui_runtime_options_t){.mock=mock,.config_path=a.config_path,.log_path="/dev/null"};
 (void)cog_tui_settings_defaults(&a.settings);cog_result_t loaded=cog_tui_settings_load(a.settings_path,&a.settings);
 if(mock&&loaded==COG_ERROR_NOT_FOUND)strcpy(a.settings.model,"mock");
 int saved_out=dup(STDOUT_FILENO),saved_err=dup(STDERR_FILENO),sink=open("/dev/null",O_WRONLY);
 if(saved_out<0||saved_err<0||sink<0){if(saved_out>=0)close(saved_out);if(saved_err>=0)close(saved_err);if(sink>=0)close(sink);return 2;}
 a.output=fdopen(saved_out,"w");if(!a.output){close(saved_out);close(saved_err);close(sink);return 2;}
 struct termios terminal;if(tcgetattr(STDIN_FILENO,&terminal)<0){fclose(a.output);close(saved_err);close(sink);return 2;}
 FILE *old_out=rl_outstream,*old_in=rl_instream;
 struct sigaction old_int,old_term,action={0};action.sa_handler=on_signal;sigemptyset(&action.sa_mask);
 (void)sigaction(SIGINT,&action,&old_int);(void)sigaction(SIGTERM,&action,&old_term);interrupted=0;
 fflush(stdout);fflush(stderr);(void)dup2(sink,STDOUT_FILENO);(void)dup2(sink,STDERR_FILENO);close(sink);
 rl_outstream=a.output;rl_instream=stdin;int old_catch=rl_catch_signals;rl_catch_signals=0;binding=&a;
 rl_callback_handler_install("cog> ",on_line);command_help(&a);
 if(loaded==COG_OK||mock)start_runtime(&a);else{if(loaded!=COG_ERROR_NOT_FOUND)error_message(&a,"Load settings",loaded);begin_settings(&a);}
 int result=0;bool done=false;
 while(!done){
  if(interrupted){int signum=interrupted;interrupted=0;if(signum==SIGINT&&a.busy&&!a.quitting)command_cancel(&a);else a.quitting=true;}
  if(a.eof)a.quitting=true;
  if(a.line){char *line=a.line;a.line=NULL;handle_line(&a,line);free(line);}
  uint32_t wait=20;
  if(a.runtime){
   if(a.quitting){cog_result_t rc=cog_tui_runtime_shutdown(a.runtime);if(rc!=COG_OK){error_message(&a,"Shutdown",rc);result=1;}}
   cog_tui_runtime_status_t status={0};cog_result_t rc=cog_tui_runtime_step(a.runtime,&status);wait=status.wait_ms;
   if(rc!=COG_OK){error_message(&a,"Runtime",rc);result=1;a.quitting=true;}
   if(render(&a)!=COG_OK){say(&a,"Display capacity/error; shutting down safely.");result=1;a.quitting=true;}
   if(status.done){cog_result_t freed=cog_tui_runtime_free(&a.runtime);if(freed!=COG_OK)result=1;done=true;}
  }else if(a.quitting)done=true;
  if(done)break;
  struct pollfd input={.fd=STDIN_FILENO,.events=POLLIN};int timeout=wait>20?20:(int)wait;
  int ready=poll(&input,1,timeout);if(ready>0){if(input.revents&POLLIN)rl_callback_read_char();if(input.revents&(POLLHUP|POLLERR))a.quitting=true;}else if(ready<0&&errno!=EINTR){a.quitting=true;result=1;}
 }
 free(a.line);rl_callback_handler_remove();binding=NULL;rl_catch_signals=old_catch;rl_outstream=old_out;rl_instream=old_in;
 (void)tcsetattr(STDIN_FILENO,TCSANOW,&terminal);(void)sigaction(SIGINT,&old_int,NULL);(void)sigaction(SIGTERM,&old_term,NULL);
 fflush(a.output);fflush(stdout);fflush(stderr);(void)dup2(saved_out,STDOUT_FILENO);(void)dup2(saved_err,STDERR_FILENO);close(saved_err);fclose(a.output);return result;
}
