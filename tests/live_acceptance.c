/** Explicitly opted-in two-request probe. Not registered with CTest.
 * Uses runtime.h public snapshot/control APIs; never prints provider text/key.
 */
#define _POSIX_C_SOURCE 200809L
#include <cog_tui/runtime.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
typedef struct turn_result {
  size_t bytes, changes;
  unsigned long long first_ms, elapsed_ms;
  bool committed, complete, marker, ack;
  int error;
} turn_result_t;
static unsigned long long millis(void) {
  struct timespec t;
  if (clock_gettime(CLOCK_MONOTONIC, &t))
    return 0;
  return (unsigned long long)t.tv_sec * 1000u +
         (unsigned long long)t.tv_nsec / 1000000u;
}
static bool contains(const char *text, size_t bytes, const char *needle) {
  size_t n = strlen(needle);
  if (n > bytes)
    return false;
  for (size_t i = 0; i <= bytes - n; ++i)
    if (!memcmp(text + i, needle, n))
      return true;
  return false;
}
static uint64_t digest(const char *text, size_t bytes) {
  uint64_t h = UINT64_C(1469598103934665603);
  for (size_t i = 0; i < bytes; ++i) {
    h ^= (unsigned char)text[i];
    h *= UINT64_C(1099511628211);
  }
  return h;
}
static void pause_ms(uint32_t delay) {
  if (delay > 20)
    delay = 20;
  struct timespec wait = {0, (long)delay * 1000000};
  (void)nanosleep(&wait, NULL);
}
static turn_result_t turn(cog_tui_runtime_t *r, const char *prompt,
                          bool *stopped) {
  turn_result_t result = {0};
  cog_cli_display_snapshot_t snapshot;
  cog_result_t rc = cog_tui_runtime_snapshot(r, &snapshot);
  if (rc != COG_OK) {
    result.error = (int)rc;
    return result;
  }
  uint64_t baseline = 0;
  for (size_t i = 0; i < snapshot.item_count; ++i)
    if (snapshot.items[i].node_id > baseline)
      baseline = snapshot.items[i].node_id;
  unsigned long long start = millis();
  rc = cog_tui_runtime_submit(r, prompt, strlen(prompt));
  if (rc != COG_OK) {
    result.error = (int)rc;
    return result;
  }
  uint64_t prior = 0;
  bool seen = false;
  while (millis() - start < 125000u) {
    cog_tui_runtime_status_t status = {0};
    rc = cog_tui_runtime_step(r, &status);
    *stopped = status.done;
    if (rc != COG_OK) {
      result.error = (int)rc;
      break;
    }
    rc = cog_tui_runtime_snapshot(r, &snapshot);
    if (rc != COG_OK) {
      result.error = (int)rc;
      break;
    }
    for (size_t i = 0; i < snapshot.item_count; ++i) {
      const cog_cli_display_item_t *item = &snapshot.items[i];
      if (item->role != COG_MESSAGE_ROLE_ASSISTANT ||
          (item->node_id && item->node_id <= baseline))
        continue;
      uint64_t hash = digest(item->text, item->text_len);
      if (item->text_len && (!seen || hash != prior)) {
        if (!seen)
          result.first_ms = millis() - start;
        ++result.changes;
        seen = true;
        prior = hash;
      }
      result.bytes = item->text_len;
      result.marker = contains(item->text, item->text_len, "MAPLE-741");
      result.ack = contains(item->text, item->text_len, "ACK");
      if (item->node_id) {
        result.committed = true;
        result.complete = item->status == COG_MESSAGE_COMPLETE;
      }
    }
    if (result.committed || status.done)
      break;
    pause_ms(status.wait_ms);
  }
  result.elapsed_ms = millis() - start;
  if (!result.committed && !result.error)
    result.error = (int)COG_ERROR_TIMEOUT;
  return result;
}
static void print_turn(FILE *out, unsigned number, const turn_result_t *r) {
  fprintf(out,
          "{\"kind\":\"turn\",\"request\":%u,\"bytes\":%zu,\"prefix_changes\":%"
          "zu,\"first_ms\":%llu,\"elapsed_ms\":%llu,\"committed\":%s,"
          "\"complete\":%s,\"marker\":%s,\"ack\":%s,\"error\":%d}\n",
          number, r->bytes, r->changes, r->first_ms, r->elapsed_ms,
          r->committed ? "true" : "false", r->complete ? "true" : "false",
          r->marker ? "true" : "false", r->ack ? "true" : "false", r->error);
  fflush(out);
}
int main(int argc, char **argv) {
  const char *opt = getenv("COG_TUI_LIVE_ACCEPT");
  if (!opt || strcmp(opt, "1") || argc != 3 || strcmp(argv[1], "--config-dir"))
    return 2;
  char settings_path[4096], config_path[4096];
  if (snprintf(settings_path, sizeof(settings_path), "%s/settings", argv[2]) >=
          (int)sizeof(settings_path) ||
      snprintf(config_path, sizeof(config_path), "%s/cog-empty.conf",
               argv[2]) >= (int)sizeof(config_path))
    return 2;
  int config =
      open(config_path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (config < 0)
    return 2;
  close(config);
  cog_tui_settings_t settings;
  if (cog_tui_settings_load(settings_path, &settings) != COG_OK)
    return 2;
  int output = dup(STDOUT_FILENO), sink = open("/dev/null", O_WRONLY);
  if (output < 0 || sink < 0)
    return 2;
  FILE *report = fdopen(output, "w");
  if (!report)
    return 2;
  fflush(stdout);
  fflush(stderr);
  if (dup2(sink, STDOUT_FILENO) < 0 || dup2(sink, STDERR_FILENO) < 0)
    return 2;
  close(sink);
  cog_tui_runtime_options_t options = {.config_path = config_path,
                                       .log_path = "/dev/null"};
  cog_tui_runtime_t *runtime = NULL;
  cog_result_t rc = cog_tui_runtime_new(&runtime, &settings, &options);
  unsigned requests = 0;
  bool stopped = false, accepted = false;
  if (rc == COG_OK) {
    const char *prompts[] = {
        "Remember marker MAPLE-741 for our next turn. Reply only: ACK",
        "What exact marker did I ask you to remember? Reply only with that "
        "marker."};
    for (unsigned i = 0; i < 2; ++i) {
      ++requests;
      fprintf(report, "{\"kind\":\"request_start\",\"request\":%u}\n",
              requests);
      fflush(report);
      turn_result_t result = turn(runtime, prompts[i], &stopped);
      print_turn(report, requests, &result);
      if (!result.complete || result.error) {
        rc = result.error ? (cog_result_t)result.error : COG_ERROR_PROTOCOL;
        break;
      }
      if (i == 1)
        accepted = result.marker;
    }
    if (!stopped) {
      cog_result_t shutdown = cog_tui_runtime_shutdown(runtime);
      unsigned long long start = millis();
      while (shutdown == COG_OK && !stopped && millis() - start < 10000u) {
        cog_tui_runtime_status_t status = {0};
        shutdown = cog_tui_runtime_step(runtime, &status);
        stopped = status.done;
        pause_ms(status.wait_ms);
      }
      if (shutdown != COG_OK || !stopped)
        rc = shutdown != COG_OK ? shutdown : COG_ERROR_TIMEOUT;
    }
    if (stopped) {
      cog_result_t freed = cog_tui_runtime_free(&runtime);
      if (freed != COG_OK)
        rc = freed;
    }
  }
  fprintf(report,
          "{\"kind\":\"summary\",\"requests\":%u,\"authoritative_marker\":%s,"
          "\"cleanup_complete\":%s,\"error\":%d}\n",
          requests, accepted ? "true" : "false", runtime ? "false" : "true",
          (int)rc);
  fclose(report);
  return rc == COG_OK && accepted ? 0 : 1;
}
