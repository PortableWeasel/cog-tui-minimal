#define _XOPEN_SOURCE 700
#include <assert.h>
#include <cog_tui/screen.h>
#include <fcntl.h>
#include <locale.h>
#include <poll.h>
#include <pty.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
static void drain(int master, char *buffer, size_t size) {
  size_t used = 0;
  struct pollfd ready = {.fd = master, .events = POLLIN};
  while (poll(&ready, 1, 30) > 0 && (ready.revents & POLLIN)) {
    ssize_t n = read(master, buffer + used, size - used - 1);
    assert(n > 0);
    used += (size_t)n;
    assert(used < size - 1);
  }
  assert(used);
  buffer[used] = '\0';
}
int main(int argc, char **argv) {
  assert(setlocale(LC_CTYPE, "C.UTF-8"));
  assert(setenv("TERM", "xterm", 1) == 0);
  int master, slave;
  struct winsize ws = {.ws_row = 12, .ws_col = 40};
  assert(openpty(&master, &slave, NULL, NULL, &ws) == 0);
  FILE *terminal = fdopen(slave, "w+");
  assert(terminal);
  cog_tui_screen_t *screen = NULL;
  assert(cog_tui_screen_open(NULL, COG_TUI_SCREEN_ANSI, terminal, terminal) ==
         COG_ERROR_NULL_PTR);
  assert(cog_tui_screen_open(&screen, COG_TUI_SCREEN_NCURSESW, terminal,
                             terminal) == COG_ERROR_UNSUPPORTED);
  assert(!screen);
  assert(cog_tui_screen_open(&screen, COG_TUI_SCREEN_ANSI, terminal,
                             terminal) == COG_OK);
  cog_tui_screen_size_t size;
  assert(cog_tui_screen_resize(screen, &size) == COG_OK);
  assert(size.output_rows == 10 && size.output_columns == 39);
  cog_tui_screen_input_t input = {
      .label = "cog> ", .bytes = "draft", .byte_count = 5, .cursor_byte = 3};
  assert(cog_tui_screen_input(screen, &input) == COG_OK);
  input.cursor_byte = 6;
  assert(cog_tui_screen_input(screen, &input) == COG_ERROR_INVALID_ARG);
  input.cursor_byte = 0;
  input.byte_count = 1048577;
  assert(cog_tui_screen_input(screen, &input) == COG_ERROR_LIMIT);
  cog_tui_md_run_t run = {
      .text = "Hello", .bytes = 5, .cells = 5, .style = COG_TUI_MD_BOLD};
  cog_tui_md_row_t row = {.runs = &run, .run_count = 1};
  cog_tui_md_view_t view = {.rows = &row, .row_count = 1, .columns = 39};
  cog_tui_screen_slice_t slice = {.view = &view, .label = "assistant:"};
  cog_tui_screen_scene_t scene = {.slices = &slice, .slice_count = 1};
  assert(cog_tui_screen_present(screen, &scene) == COG_OK);
  char buffer[32768];
  drain(master, buffer, sizeof(buffer));
  if (argc > 1 && !strcmp(argv[1], "basic"))
    fputs(buffer, stdout);
  assert(strstr(buffer, "assistant:"));
  assert(strstr(buffer, "Hello"));
  assert(strstr(buffer, "--- input ---"));
  assert(strstr(buffer, "\033[12;9H"));
  run.text = "\033[2J";
  run.bytes = 4;
  run.cells = 4;
  assert(cog_tui_screen_present(screen, &scene) == COG_ERROR_INVALID_ARG);
  run.text = "Hello";
  run.bytes = 5;
  run.cells = 4;
  assert(cog_tui_screen_present(screen, &scene) == COG_ERROR_INVALID_ARG);
  run.cells = 5;
  input = (cog_tui_screen_input_t){.label = "cog> ",
                                   .bytes = "\033\xc2\x9b\xe7\x95\x8c"
                                            "e\xcc\x81",
                                   .byte_count = 9,
                                   .cursor_byte = 5};
  assert(cog_tui_screen_input(screen, &input) == COG_OK);
  assert(cog_tui_screen_present(screen, &scene) == COG_OK);
  drain(master, buffer, sizeof(buffer));
  if (argc > 1 && !strcmp(argv[1], "unicode"))
    fputs(buffer, stdout);
  assert(strstr(buffer, "\\x1b\\u009b"));
  assert(strstr(buffer, "\\u0301"));
  assert(strstr(buffer,
                "\033[12;16H")); /* inside CJK maps to preceding boundary */
  char long_text[120];
  memset(long_text, 'a', sizeof(long_text));
  long_text[119] = 'Z';
  input = (cog_tui_screen_input_t){.label = "cog> ",
                                   .bytes = long_text,
                                   .byte_count = 120,
                                   .cursor_byte = 120};
  assert(cog_tui_screen_input(screen, &input) == COG_OK);
  assert(cog_tui_screen_present(screen, &scene) == COG_OK);
  drain(master, buffer, sizeof(buffer));
  assert(strstr(buffer, "Z"));
  assert(strstr(buffer, "\033[12;39H"));
  ws.ws_row = 2;
  ws.ws_col = 8;
  assert(ioctl(slave, TIOCSWINSZ, &ws) == 0);
  assert(cog_tui_screen_present(screen, &scene) == COG_ERROR_AGAIN);
  assert(cog_tui_screen_resize(screen, &size) == COG_OK &&
         size.output_rows == 0);
  scene = (cog_tui_screen_scene_t){0};
  assert(cog_tui_screen_present(screen, &scene) == COG_OK);
  drain(master, buffer, sizeof(buffer));
  assert(strstr(buffer, "Resize "));
  assert(cog_tui_screen_close(&screen) == COG_OK && !screen);
  drain(master, buffer, sizeof(buffer));
  assert(strstr(buffer, "\033[r"));
  assert(strstr(buffer, "\033[?25h"));
  assert(cog_tui_screen_close(&screen) == COG_OK);
  fclose(terminal);
  close(master);
  return 0;
}
