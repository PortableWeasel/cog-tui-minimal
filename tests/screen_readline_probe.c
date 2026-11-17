#define _POSIX_C_SOURCE 200809L
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static int done;
static void redisplay(void) {}
static void line(char *s) { free(s); done = 1; }
int main(void) {
 FILE *capture = tmpfile(); if (!capture) return 2;
 rl_outstream = capture; rl_catch_signals = 0;
 rl_redisplay_function = redisplay;
 rl_callback_handler_install("probe> ", line);
 long start = ftell(capture);
 while (!done) rl_callback_read_char();
 rl_callback_handler_remove();
 fflush(capture); long end = ftell(capture);
 fprintf(stderr, "hidden-bytes=%ld\n", end-start);
 rewind(capture); int c; while ((c=fgetc(capture)) != EOF) fprintf(stderr,"%02x ", (unsigned)c);
 fputc('\n',stderr); fclose(capture); return 0;
}
