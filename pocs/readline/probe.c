#define _POSIX_C_SOURCE 200809L
#include <readline/readline.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Readline callbacks lack a user-data argument. This PoC has one binding. */
static char *accepted;
static void line_ready(char *line) { accepted=line; rl_callback_handler_remove(); }
int main(void) {
 rl_callback_handler_install("cog> ",line_ready);
 int ticks=0;
 while(!accepted) {
  struct pollfd input={.fd=0,.events=POLLIN};
  int ready=poll(&input,1,50);
  if(ready<0) return 2;
  if(ready>0) rl_callback_read_char();
  if(++ticks==5 && !accepted) {
   rl_save_prompt();
   rl_replace_line(rl_line_buffer,0);
   rl_crlf();
   puts("streamed answer");
   rl_restore_prompt();
   rl_on_new_line();
   rl_redisplay();
  }
 }
 int result=strcmp(accepted,"draft text");
 printf("accepted=%s\n",accepted);
 free(accepted);
 return result ? 1:0;
}
