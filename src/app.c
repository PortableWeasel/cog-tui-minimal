#define _POSIX_C_SOURCE 200809L
#include <cog_tui/app.h>
#include <cog_tui/runtime.h>
#include <cog_tui/transcript.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <poll.h>
#include <readline/keymaps.h>
#include <readline/readline.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

typedef struct app {
  FILE *output;
  cog_tui_settings_t settings, edit;
  cog_tui_runtime_t *runtime;
  cog_tui_runtime_options_t options;
  char settings_path[4096], config_path[4096];
  char *line;
  bool eof, quitting, busy, editing;
  unsigned field;
  cog_tui_markdown_t *markdown;
  cog_tui_screen_t *screen;
  cog_tui_transcript_t *transcript;
  cog_tui_screen_size_t size;
  cog_result_t display_error;
  bool dirty, immediate;
  uint64_t last_paint, last_terminal;
  char notices[8][512];
  size_t notice_count;
  char runtime_model[COG_TUI_VALUE_CAPACITY],
      published_model[COG_TUI_VALUE_CAPACITY];
  bool pending_selection;
  uint64_t baseline;

} app_t;
/* Readline exposes no callback user-data. One bounded lifetime binding only. */
static app_t *binding;
static volatile sig_atomic_t interrupted;
static void on_signal(int signo) { interrupted = signo; }
static void on_line(char *line) {
  if (binding) {
    binding->line = line;
    if (!line)
      binding->eof = true;
  } else
    free(line);
}
static uint64_t milliseconds(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    return 0;
  return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}
static void input_redraw(app_t *a) {
  if (!a->screen)
    return;
  const char *draft = rl_line_buffer ? rl_line_buffer : "";
  size_t count = strlen(draft);
  size_t cursor = rl_point >= 0 ? (size_t)rl_point : 0;
  if (cursor > count)
    cursor = count;
  cog_tui_screen_input_t input = {rl_prompt ? rl_prompt : "cog> ", draft, count,
                                  cursor};
  cog_result_t rc = cog_tui_screen_input(a->screen, &input);
  if (rc != COG_OK)
    a->display_error = rc;
  a->dirty = true;
  a->immediate = true;
}
static void redisplay(void) {
  if (binding)
    input_redraw(binding);
}
static int scroll_back(int count, int key) {
  (void)count;
  (void)key;
  if (binding) {
    (void)cog_tui_transcript_scroll(binding->transcript, -1);
    binding->dirty = true;
    binding->immediate = true;
  }
  return 0;
}
static int scroll_forward(int count, int key) {
  (void)count;
  (void)key;
  if (binding) {
    (void)cog_tui_transcript_scroll(binding->transcript, 1);
    binding->dirty = true;
    binding->immediate = true;
  }
  return 0;
}
static int scroll_tail(int count, int key) {
  (void)count;
  (void)key;
  if (binding) {
    (void)cog_tui_transcript_scroll(binding->transcript, 0);
    binding->dirty = true;
    binding->immediate = true;
  }
  return 0;
}
static void layout(app_t *a) {
  cog_tui_screen_size_t size;
  cog_result_t rc = cog_tui_screen_resize(a->screen, &size);
  if (rc != COG_OK) {
    a->display_error = rc;
    return;
  }
  if (size.rows != a->size.rows || size.columns != a->size.columns) {
    a->size = size;
    rl_resize_terminal();
    a->dirty = true;
    a->immediate = true;
  }
}
static void say(app_t *a, const char *text) {
  while (*text) {
    if (a->notice_count == 8) {
      memmove(a->notices, a->notices + 1, 7 * sizeof(a->notices[0]));
      a->notice_count = 7;
    }
    size_t n = strlen(text);
    if (n > 511)
      n = 511;
    memcpy(a->notices[a->notice_count], text, n);
    a->notices[a->notice_count][n] = '\0';
    ++a->notice_count;
    text += n;
  }
  a->dirty = true;
  a->immediate = true;
}
static void error_message(app_t *a, const char *operation, cog_result_t rc) {
  char msg[160];
  (void)snprintf(
      msg, sizeof(msg),
      "%s failed (Cog error %d). Use /settings to correct configuration.",
      operation, (int)rc);
  say(a, msg);
}
static void prompt(app_t *a) {
  const char *p = "cog> ";
  if (a->editing) {
    static const char *const prompts[] = {
        "Model: ", "HTTPS base URL (blank keeps current): ",
        "Credential reference env:NAME (blank keeps current): "};
    p = prompts[a->field];
  }
  rl_set_prompt(p);
  input_redraw(a);
}
static void start_runtime(app_t *a) {
  cog_result_t rc = cog_tui_runtime_new(&a->runtime, &a->settings, &a->options);
  if (rc != COG_OK) {
    error_message(a, "Runtime setup", rc);
    say(a, "Check that the credential environment variable is set and the "
           "installed Cog dependency profile is compatible.");
  } else {
    strcpy(a->runtime_model, a->settings.model);
    a->pending_selection = false;
    strcpy(a->published_model, a->settings.model);
    say(a, a->options.mock ? "MOCK ready (offline fixture)."
                           : "Ready. Provider: openrouter. Network "
                             "authentication is checked on first request.");
    char model[1100];
    (void)snprintf(model, sizeof(model), "Runtime model selection: %s",
                   a->runtime_model);
    say(a, model);
  }
}
static void begin_settings(app_t *a) {
  if (a->busy) {
    say(a, "Finish or cancel the current turn before changing settings.");
    return;
  }
  a->edit = a->settings;
  a->editing = true;
  a->field = 0;
  say(a,
      "Provider: openrouter. Enter a model ID; blank keeps the current value. "
      "Credentials use an existing environment variable, never a raw key.");
  prompt(a);
}
static void accept_setting(app_t *a, const char *line) {
  char *dest = a->field == 0   ? a->edit.model
               : a->field == 1 ? a->edit.base_url
                               : a->edit.credential_ref;
  if (strlen(line) >= COG_TUI_VALUE_CAPACITY) {
    say(a, "Value is too long.");
    return;
  }
  if (*line)
    strcpy(dest, line);
  if (a->field < 2) {
    a->field++;
    prompt(a);
    return;
  }
  cog_result_t rc = cog_tui_settings_validate(&a->edit);
  if (rc != COG_OK) {
    error_message(a, "Settings validation", rc);
    a->field = 0;
    prompt(a);
    return;
  }
  rc = cog_tui_settings_save(a->settings_path, &a->edit);
  if (rc != COG_OK) {
    error_message(a, "Settings save", rc);
    return;
  }
  a->settings = a->edit;
  a->editing = false;
  say(a, "Settings saved.");
  if (a->runtime) {
    rc = cog_tui_runtime_reload(a->runtime, &a->settings);
    if (rc != COG_OK)
      error_message(a, "Apply saved settings", rc);
    else {
      a->pending_selection = true;
      strcpy(a->published_model, a->settings.model);
      say(a, "Selection published for the next turn; current conversation "
             "retained.");
    }
  } else
    start_runtime(a);
  prompt(a);
}
static void command_quit(app_t *a) { a->quitting = true; }
static void command_cancel(app_t *a) {
  if (!a->runtime || !a->busy) {
    say(a, "No active turn.");
    return;
  }
  cog_result_t rc = cog_tui_runtime_cancel(a->runtime);
  if (rc != COG_OK)
    error_message(a, "Cancel", rc);
  else
    say(a, "Cancellation requested; waiting for Cog outcome.");
}
static void command_show(app_t *a) {
  char msg[2300];
  (void)snprintf(msg, sizeof(msg),
                 "Saved selection: provider=%s model=%s\nEndpoint=%s",
                 a->settings.provider, a->settings.model, a->settings.base_url);
  say(a, msg);
  if (a->runtime) {
    (void)snprintf(
        msg, sizeof(msg), "Last published runtime model: %s%s",
        a->published_model,
        a->pending_selection
            ? " (pending next turn; activation unconfirmed)"
            : " (initial/previous selection; activation not queried)");
    say(a, msg);
  }
}
static void command_help(app_t *a) {
  say(a, "/settings configure provider details and model; /show saved "
         "selection; /cancel active turn; /quit exit. Ctrl-C cancels a turn, "
         "or exits when idle. Ctrl-P/Ctrl-N scroll; Ctrl-T returns live. "
         "Tools are disabled in this version.");
}
static void handle_line(app_t *a, const char *line) {
  if (a->editing) {
    accept_setting(a, line);
    return;
  }
  static const struct {
    const char *name;
    void (*run)(app_t *);
  } commands[] = {{"/quit", command_quit},
                  {"/cancel", command_cancel},
                  {"/settings", begin_settings},
                  {"/show", command_show},
                  {"/help", command_help}};
  for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
    if (!strcmp(line, commands[i].name)) {
      commands[i].run(a);
      return;
    }
  if (!*line)
    return;
  if (line[0] == '/') {
    say(a, "Unknown command. Use /help.");
    return;
  }
  if (!a->runtime) {
    say(a, "Runtime unavailable. Use /settings to correct and retry setup.");
    return;
  }
  if (a->busy) {
    say(a, "A turn is active. Wait or use /cancel; this prompt was not "
           "submitted.");
    return;
  }
  cog_cli_display_snapshot_t v;
  cog_result_t rc = cog_tui_runtime_snapshot(a->runtime, &v);
  if (rc != COG_OK) {
    error_message(a, "Snapshot", rc);
    return;
  }
  a->baseline = 0;
  for (size_t i = 0; i < v.item_count; i++)
    if (v.items[i].node_id > a->baseline)
      a->baseline = v.items[i].node_id;
  rc = cog_tui_runtime_submit(a->runtime, line, strlen(line));
  if (rc != COG_OK)
    error_message(a, "Submit", rc);
  else {
    a->busy = true;
    a->notice_count = 0;
    a->dirty = true;
    a->immediate = true;
  }
}
static cog_result_t paint(app_t *a,
                          const cog_cli_display_snapshot_t *snapshot) {
  if (a->display_error != COG_OK)
    return a->display_error;
  uint64_t now = milliseconds();
  if (!a->dirty || (!a->immediate && now - a->last_paint < 34))
    return COG_OK;
  if (!a->size.output_rows) {
    cog_tui_screen_scene_t empty = {0};
    cog_result_t rc = cog_tui_screen_present(a->screen, &empty);
    if (rc == COG_ERROR_AGAIN) {
      a->immediate = true;
      return COG_OK;
    }
    return rc;
  }
  cog_tui_screen_slice_t slices[300];
  cog_tui_md_view_t notices[8];
  cog_tui_md_frame_t *frames[8] = {0};
  size_t n = 0, notice_rows = 0, notice_frames = 0;
  size_t notice_budget = snapshot && snapshot->item_count
                             ? a->size.output_rows / 2
                             : a->size.output_rows;
  cog_result_t rc = COG_OK;
  for (size_t i = a->notice_count; i > 0 && notice_rows < notice_budget; --i) {
    cog_tui_md_source_t source = {a->notices[i - 1], strlen(a->notices[i - 1]),
                                  UINT64_MAX - i, COG_TUI_MD_COMPLETE};
    cog_tui_md_window_t window = {a->size.output_columns, 0,
                                  notice_budget - notice_rows};
    rc = cog_tui_markdown_render(a->markdown, &source, COG_TUI_MD_FORMAT_PLAIN,
                                 &window, &frames[notice_frames]);
    if (rc != COG_OK)
      goto cleanup;
    (void)cog_tui_md_frame_view(frames[notice_frames], &notices[notice_frames]);
    notice_rows += notices[notice_frames].row_count;
    ++notice_frames;
  }
  cog_tui_screen_scene_t conversation = {0};
  if (snapshot && a->size.output_rows > notice_rows) {
    rc = cog_tui_transcript_compose(a->transcript, snapshot,
                                    a->size.output_rows - notice_rows,
                                    a->size.output_columns, &conversation);
    if (rc != COG_OK)
      goto cleanup;
    for (size_t i = 0; i < conversation.slice_count; ++i)
      slices[n++] = conversation.slices[i];
  }
  for (size_t i = notice_frames; i > 0; --i)
    slices[n++] = (cog_tui_screen_slice_t){&notices[i - 1], NULL};
  cog_tui_screen_scene_t scene = {slices, n, conversation.status};
  rc = cog_tui_screen_present(a->screen, &scene);
  if (rc == COG_ERROR_AGAIN) {
    a->immediate = true;
    rc = COG_OK;
    goto cleanup;
  }
  if (rc == COG_OK) {
    a->dirty = false;
    a->immediate = false;
    a->last_paint = now;
  }
cleanup:
  for (size_t i = 0; i < 8; ++i)
    (void)cog_tui_md_frame_free(&frames[i]);
  return rc;
}
static cog_result_t render(app_t *a) {
  if (!a->runtime)
    return paint(a, NULL);
  cog_cli_display_snapshot_t v;
  cog_result_t rc = cog_tui_runtime_snapshot(a->runtime, &v);
  if (rc != COG_OK)
    return rc;
  bool changed = false;
  rc = cog_tui_transcript_sync(a->transcript, &v, &changed);
  if (rc != COG_OK)
    return rc;
  a->dirty |= changed;
  for (size_t i = 0; i < v.item_count; ++i) {
    const cog_cli_display_item_t *item = &v.items[i];
    if (item->role != COG_MESSAGE_ROLE_ASSISTANT || !item->node_id ||
        item->node_id <= a->last_terminal)
      continue;
    a->last_terminal = item->node_id;
    a->immediate = true;
    if (item->node_id > a->baseline) {
      a->busy = false;
      if (a->pending_selection) {
        strcpy(a->runtime_model, a->published_model);
        a->pending_selection = false;
      }
    }
    if (item->status == COG_MESSAGE_CANCELLED)
      say(a, "Turn cancelled.");
    else if (item->status != COG_MESSAGE_COMPLETE)
      say(a, a->quitting ? "Turn incomplete or failed. Runtime stopped; "
                           "restart to continue. No automatic replay."
                         : "Turn incomplete or failed. Submit fresh input to "
                           "continue; no automatic replay.");
  }
  return paint(a, &v);
}
static int private_directory(const char *path) {
  struct stat st;
  if (mkdir(path, 0700) < 0 && errno != EEXIST)
    return -1;
  if (lstat(path, &st) < 0 || !S_ISDIR(st.st_mode) || st.st_uid != geteuid() ||
      (st.st_mode & 077) != 0)
    return -1;
  return 0;
}
int cog_tui_app_run(int argc, char **argv) {
  if (argc < 1 || !argv)
    return 2;
  bool mock = false;
  const char *directory = NULL;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--help")) {
      puts("Usage: cog-tui [--mock] [--config-dir DIRECTORY]\nInteractive "
           "readline client for installed Cog. --mock is an offline test "
           "fixture.");
      return 0;
    } else if (!strcmp(argv[i], "--mock"))
      mock = true;
    else if (!strcmp(argv[i], "--config-dir") && i + 1 < argc)
      directory = argv[++i];
    else {
      fputs("Invalid option; use --help.\n", stderr);
      return 2;
    }
  }
  if (binding || !isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
    fputs("cog-tui requires an interactive terminal.\n", stderr);
    return 2;
  }
  app_t a = {0};
  char dir[4096];
  if (!directory) {
    const char *home = getenv("HOME");
    if (!home ||
        snprintf(dir, sizeof(dir), "%s/.cog-tui", home) >= (int)sizeof(dir))
      return 2;
    directory = dir;
  }
  if (private_directory(directory) < 0) {
    fputs("Configuration directory must be private and owned by you.\n",
          stderr);
    return 2;
  }
  if (snprintf(a.settings_path, sizeof(a.settings_path), "%s/settings",
               directory) >= (int)sizeof(a.settings_path) ||
      snprintf(a.config_path, sizeof(a.config_path), "%s/cog-empty.conf",
               directory) >= (int)sizeof(a.config_path))
    return 2;
  int config =
      open(a.config_path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (config >= 0)
    close(config);
  else if (errno != EEXIST)
    return 2;
  struct stat st;
  if (lstat(a.config_path, &st) < 0 || !S_ISREG(st.st_mode) ||
      st.st_uid != geteuid() || (st.st_mode & 077) != 0 || st.st_size != 0) {
    fputs("Unsafe or nonempty UI runtime config file.\n", stderr);
    return 2;
  }
  a.options = (cog_tui_runtime_options_t){
      .mock = mock, .config_path = a.config_path, .log_path = "/dev/null"};
  (void)cog_tui_settings_defaults(&a.settings);
  cog_result_t loaded = cog_tui_settings_load(a.settings_path, &a.settings);
  if (mock && loaded == COG_ERROR_NOT_FOUND)
    strcpy(a.settings.model, "mock");
  int saved_out = dup(STDOUT_FILENO), saved_err = dup(STDERR_FILENO),
      sink = open("/dev/null", O_WRONLY);
  if (saved_out < 0 || saved_err < 0 || sink < 0) {
    if (saved_out >= 0)
      close(saved_out);
    if (saved_err >= 0)
      close(saved_err);
    if (sink >= 0)
      close(sink);
    return 2;
  }
  a.output = fdopen(saved_out, "w");
  if (!a.output) {
    close(saved_out);
    close(saved_err);
    close(sink);
    return 2;
  }
  struct termios terminal;
  if (tcgetattr(STDIN_FILENO, &terminal) < 0) {
    fclose(a.output);
    close(saved_err);
    close(sink);
    return 2;
  }
  FILE *old_out = rl_outstream, *old_in = rl_instream;
  struct sigaction old_int, old_term, action = {0};
  action.sa_handler = on_signal;
  sigemptyset(&action.sa_mask);
  if (sigaction(SIGINT, &action, &old_int) < 0) {
    fclose(a.output);
    close(saved_err);
    close(sink);
    return 2;
  }
  if (sigaction(SIGTERM, &action, &old_term) < 0) {
    (void)sigaction(SIGINT, &old_int, NULL);
    fclose(a.output);
    close(saved_err);
    close(sink);
    return 2;
  }
  interrupted = 0;
  fflush(stdout);
  fflush(stderr);
  if (dup2(sink, STDOUT_FILENO) < 0 || dup2(sink, STDERR_FILENO) < 0) {
    (void)dup2(saved_out, STDOUT_FILENO);
    (void)dup2(saved_err, STDERR_FILENO);
    (void)sigaction(SIGINT, &old_int, NULL);
    (void)sigaction(SIGTERM, &old_term, NULL);
    fclose(a.output);
    close(saved_err);
    close(sink);
    return 2;
  }
  close(sink);
  FILE *readline_sink = fopen("/dev/null", "w");
  (void)setlocale(LC_CTYPE, "");
  if (MB_CUR_MAX == 1)
    (void)setlocale(LC_CTYPE, "C.UTF-8");
  cog_result_t init =
      readline_sink ? cog_tui_markdown_new(&a.markdown, NULL) : COG_ERROR_IO;
  if (init == COG_OK)
    init = cog_tui_transcript_new(&a.transcript, a.markdown);
  if (init == COG_OK)
    init = cog_tui_screen_open(&a.screen, COG_TUI_SCREEN_ANSI, stdin, a.output);
  if (init != COG_OK) {
    (void)cog_tui_transcript_free(&a.transcript);
    (void)cog_tui_markdown_free(&a.markdown);
    if (readline_sink)
      fclose(readline_sink);
    (void)dup2(saved_out, STDOUT_FILENO);
    (void)dup2(saved_err, STDERR_FILENO);
    (void)sigaction(SIGINT, &old_int, NULL);
    (void)sigaction(SIGTERM, &old_term, NULL);
    close(saved_err);
    fclose(a.output);
    return 2;
  }
  rl_voidfunc_t *old_redisplay = rl_redisplay_function;
  rl_redisplay_function = redisplay;
  const char *bell = rl_variable_value("bell-style");
  char old_bell[32];
  (void)snprintf(old_bell, sizeof(old_bell), "%s", bell ? bell : "audible");
  (void)rl_variable_bind("bell-style", "none");
  rl_outstream = readline_sink;
  rl_instream = stdin;
  int old_catch = rl_catch_signals;
  rl_catch_signals = 0;
  const char *horizontal = rl_variable_value("horizontal-scroll-mode");
  bool old_horizontal = horizontal && !strcmp(horizontal, "on");
  (void)rl_variable_bind("horizontal-scroll-mode", "on");
  binding = &a;
  rl_callback_handler_install("cog> ", on_line);
  const char *paste = rl_variable_value("enable-bracketed-paste");
  bool old_paste = paste && !strcmp(paste, "on");
  (void)rl_variable_bind("enable-bracketed-paste", "on");
  Keymap old_map = rl_get_keymap();
  KEYMAP_ENTRY old_back = old_map[16], old_forward = old_map[14],
               old_tail = old_map[20];
  old_map[16] = (KEYMAP_ENTRY){ISFUNC, scroll_back};
  old_map[14] = (KEYMAP_ENTRY){ISFUNC, scroll_forward};
  old_map[20] = (KEYMAP_ENTRY){ISFUNC, scroll_tail};
  input_redraw(&a);
  layout(&a);
  command_help(&a);
  if (loaded == COG_OK || mock)
    start_runtime(&a);
  else {
    if (loaded != COG_ERROR_NOT_FOUND)
      error_message(&a, "Load settings", loaded);
    begin_settings(&a);
  }
  int result = 0;
  bool done = false;
  while (!done) {
    layout(&a);
    if (interrupted) {
      int signum = interrupted;
      interrupted = 0;
      if (signum == SIGINT && a.busy && !a.quitting)
        command_cancel(&a);
      else
        a.quitting = true;
    }
    if (a.eof)
      a.quitting = true;
    if (a.line) {
      char *line = a.line;
      a.line = NULL;
      handle_line(&a, line);
      free(line);
    }
    uint32_t wait = 20;
    if (a.runtime) {
      if (a.quitting) {
        cog_result_t rc = cog_tui_runtime_shutdown(a.runtime);
        if (rc != COG_OK) {
          error_message(&a, "Shutdown", rc);
          result = 1;
        }
      }
      cog_tui_runtime_status_t status = {0};
      cog_result_t rc = cog_tui_runtime_step(a.runtime, &status);
      wait = status.wait_ms;
      if (rc != COG_OK) {
        (void)cog_tui_transcript_fail(a.transcript);
        error_message(&a, "Runtime", rc);
        result = 1;
        a.quitting = true;
      }
      if (render(&a) != COG_OK) {
        say(&a, "Display capacity/error; shutting down safely.");
        result = 1;
        a.quitting = true;
      }
      if (status.done) {
        cog_result_t freed = cog_tui_runtime_free(&a.runtime);
        if (freed != COG_OK)
          result = 1;
        done = true;
      }
    } else {
      if (render(&a) != COG_OK) {
        a.quitting = true;
        result = 1;
      }
      if (a.quitting)
        done = true;
    }
    if (done)
      break;
    struct pollfd input = {.fd = STDIN_FILENO, .events = POLLIN};
    int timeout = wait > 20 ? 20 : (int)wait;
    int ready = poll(&input, 1, timeout);
    if (ready > 0) {
      if (input.revents & POLLIN) {
        rl_callback_read_char();
        input_redraw(&a);
      }
      if (input.revents & (POLLHUP | POLLERR))
        a.quitting = true;
    } else if (ready < 0 && errno != EINTR) {
      a.quitting = true;
      result = 1;
    }
  }
  free(a.line);
  rl_callback_handler_remove();
  (void)rl_variable_bind("enable-bracketed-paste", old_paste ? "on" : "off");
  old_map[16] = old_back;
  old_map[14] = old_forward;
  old_map[20] = old_tail;
  rl_redisplay_function = old_redisplay;
  (void)rl_variable_bind("bell-style", old_bell);
  if (cog_tui_screen_close(&a.screen) != COG_OK)
    result = 1;
  (void)cog_tui_transcript_free(&a.transcript);
  (void)cog_tui_markdown_free(&a.markdown);
  fclose(readline_sink);
  (void)rl_variable_bind("horizontal-scroll-mode",
                         old_horizontal ? "on" : "off");
  binding = NULL;
  rl_catch_signals = old_catch;
  rl_outstream = old_out;
  rl_instream = old_in;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &terminal) < 0)
    result = 1;
  if (sigaction(SIGINT, &old_int, NULL) < 0)
    result = 1;
  if (sigaction(SIGTERM, &old_term, NULL) < 0)
    result = 1;
  if (fflush(a.output) != 0)
    result = 1;
  fflush(stdout);
  fflush(stderr);
  if (dup2(saved_out, STDOUT_FILENO) < 0)
    result = 1;
  if (dup2(saved_err, STDERR_FILENO) < 0)
    result = 1;
  close(saved_err);
  if (fclose(a.output) != 0)
    result = 1;
  return result;
}
