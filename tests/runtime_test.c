#define _POSIX_C_SOURCE 200809L
#include <cog_tui/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define REQUIRE(x)                                                             \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "line %d\n", __LINE__);                                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)
int main(void) {
  char path[] = "runtime-config-XXXXXX";
  int fd = mkstemp(path);
  REQUIRE(fd >= 0);
  close(fd);
  cog_tui_settings_t settings;
  REQUIRE(cog_tui_settings_defaults(&settings) == COG_OK);
  strcpy(settings.model, "mock");
  cog_tui_runtime_options_t opts = {
      .mock = true, .config_path = path, .log_path = "/dev/null"};
  cog_tui_runtime_t *runtime = NULL;
  REQUIRE(cog_tui_runtime_new(&runtime, &settings, &opts) == COG_OK);
  cog_cli_display_snapshot_t v;
  REQUIRE(cog_tui_runtime_snapshot(runtime, &v) == COG_OK);
  REQUIRE(v.item_count == 0);
  for (unsigned turn = 0; turn < 2; turn++) {
    REQUIRE(cog_tui_runtime_submit(runtime, "hello", 5) == COG_OK);
    REQUIRE(cog_tui_runtime_submit(runtime, "duplicate", 9) ==
            COG_ERROR_INVALID_STATE);
    bool found = false;
    for (unsigned i = 0; i < 10000 && !found; i++) {
      struct timespec delay = {.tv_nsec = 1000000};
      nanosleep(&delay, NULL);
      cog_tui_runtime_status_t s;
      REQUIRE(cog_tui_runtime_step(runtime, &s) == COG_OK);
      REQUIRE(!s.done);
      REQUIRE(cog_tui_runtime_snapshot(runtime, &v) == COG_OK);
      found = v.item_count >= (size_t)(turn + 1) * 2 &&
              v.items[v.item_count - 1].node_id != 0;
    }
    REQUIRE(found);
  }
  strcpy(settings.model, "mock-next");
  REQUIRE(cog_tui_runtime_reload(runtime, &settings) == COG_OK);
  REQUIRE(cog_tui_runtime_submit(runtime, "cancel", 6) == COG_OK);
  REQUIRE(cog_tui_runtime_cancel(runtime) == COG_OK);
  REQUIRE(cog_tui_runtime_free(&runtime) == COG_ERROR_INVALID_STATE);
  REQUIRE(runtime != NULL);
  REQUIRE(cog_tui_runtime_shutdown(runtime) == COG_OK);
  cog_tui_runtime_status_t s = {0};
  for (unsigned i = 0; i < 10000 && !s.done; i++)
    REQUIRE(cog_tui_runtime_step(runtime, &s) == COG_OK);
  REQUIRE(s.done);
  REQUIRE(cog_tui_runtime_free(&runtime) == COG_OK);
  REQUIRE(runtime == NULL);
  REQUIRE(cog_tui_runtime_free(&runtime) == COG_OK);
  REQUIRE(setenv("COG_TUI_OFFLINE_KEY", "test-only-not-a-real-key", 1) == 0);
  strcpy(settings.credential_ref, "env:COG_TUI_OFFLINE_KEY");
  opts.mock = false;
  cog_result_t real_result = cog_tui_runtime_new(&runtime, &settings, &opts);
  fprintf(stderr, "REAL constructor: %d\n", real_result);
  REQUIRE(real_result == COG_OK);
  REQUIRE(cog_tui_runtime_snapshot(runtime, &v) == COG_OK);
  REQUIRE(v.item_count == 0);
  REQUIRE(cog_tui_runtime_shutdown(runtime) == COG_OK);
  s.done = false;
  for (unsigned i = 0; i < 10000 && !s.done; i++)
    REQUIRE(cog_tui_runtime_step(runtime, &s) == COG_OK);
  REQUIRE(s.done);
  REQUIRE(cog_tui_runtime_free(&runtime) == COG_OK);
  unsetenv("COG_TUI_OFFLINE_KEY");
  unlink(path);
  return 0;
}
