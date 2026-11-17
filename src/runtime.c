#include <cog_tui/runtime.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
struct cog_tui_runtime {
  cog_cli_app_t *app;
  cog_net_ctx_t *net;
  cog_tui_settings_t selected;
  bool curl_owned, mock, done, shutting, busy, driving;
  uint64_t baseline;
  cog_result_t result;
};
static void release(cog_tui_runtime_t *r) {
  cog_cli_app_free(r->app);
  cog_net_free(r->net);
  if (r->curl_owned)
    curl_global_cleanup();
  free(r);
}
cog_result_t cog_tui_runtime_new(cog_tui_runtime_t **out,
                                 const cog_tui_settings_t *s,
                                 const cog_tui_runtime_options_t *o) {
  if (!out)
    return COG_ERROR_NULL_PTR;
  *out = NULL;
  if (!s || !o || !o->config_path)
    return COG_ERROR_NULL_PTR;
  cog_result_t rc = cog_tui_settings_validate(s);
  if (rc != COG_OK)
    return rc;
  cog_tui_runtime_t *r = calloc(1, sizeof(*r));
  if (!r)
    return COG_ERROR_NOMEM;
  r->selected = *s;
  r->mock = o->mock;
  if (!o->mock) {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
      free(r);
      return COG_ERROR_IO;
    }
    r->curl_owned = true;
    cog_net_config_t net = {.host_runtime_initialized = true,
                            .dependency_memory =
                                COG_NET_DEPENDENCY_MEMORY_DYNAMIC,
                            .max_connections = 2,
                            .max_request_bytes = 1048576,
                            .connect_timeout_ms = 10000,
                            .stall_timeout_ms = 30000};
    rc = cog_net_new(&r->net, &net);
    if (rc != COG_OK) {
      release(r);
      return rc;
    }
  }
  static const cog_mock_step_t mock_steps[] = {
      {.kind = COG_MOCK_STEP_CHUNK, .chunk = "Hello "},
      {.kind = COG_MOCK_STEP_CHUNK, .chunk = "from cog-cli!"},
      {.kind = COG_MOCK_STEP_COMPLETE}};
  static const cog_mock_script_t mock_script = {.steps = mock_steps,
                                                .step_count = 3,
                                                .repeat = SIZE_MAX,
                                                .chunk_delay_ms = 100};
  cog_cli_options_t opts = {
      .script = o->mock ? &mock_script : NULL,
      .backend = o->mock ? COG_CLI_BACKEND_MOCK : COG_CLI_BACKEND_REAL,
      .net = r->net,
      .model_id = s->model,
      .endpoint_name = o->mock ? "mock" : "openrouter",
      .base_url = s->base_url,
      .credential_ref = s->credential_ref,
      .config_path = o->config_path,
      .log_path = o->log_path,
      .store_mode = COG_CLI_STORE_DISABLED,
      .tool_policy = COG_CLI_TOOL_POLICY_DENY,
      .interactive = false,
      .max_output_tokens = 4096,
      .max_context_bytes = 1048576,
      .max_turn_ms = 120000,
      .max_retries = 0,
      .explicit_options = COG_CLI_OPT_BACKEND | COG_CLI_OPT_MODEL |
                          COG_CLI_OPT_ENDPOINT | COG_CLI_OPT_BASE_URL |
                          COG_CLI_OPT_CREDENTIAL | COG_CLI_OPT_STORE_MODE |
                          COG_CLI_OPT_POLICY | COG_CLI_OPT_OUTPUT_TOKENS |
                          COG_CLI_OPT_CONTEXT | COG_CLI_OPT_TURN_MS |
                          COG_CLI_OPT_RETRIES};
  rc = cog_cli_app_new(&r->app, &opts);
  if (rc == COG_OK)
    rc = cog_cli_app_run(
        r->app); /* Empty run: establishes public teardown precondition. */
  if (rc == COG_OK)
    rc = cog_core_drive_begin(cog_cli_app_core(r->app));
  if (rc != COG_OK) {
    release(r);
    return rc;
  }
  r->driving = true;
  *out = r;
  return COG_OK;
}
cog_result_t cog_tui_runtime_submit(cog_tui_runtime_t *r, const char *text,
                                    size_t len) {
  if (!r || !text)
    return COG_ERROR_NULL_PTR;
  if (r->done || r->shutting || r->busy)
    return COG_ERROR_INVALID_STATE;
  if (!len || memchr(text, 0, len))
    return COG_ERROR_INVALID_ARG;
  cog_cli_display_snapshot_t view;
  cog_result_t rc = cog_cli_app_display_snapshot(r->app, &view);
  if (rc != COG_OK)
    return rc;
  uint64_t max = 0;
  for (size_t i = 0; i < view.item_count; i++)
    if (view.items[i].node_id > max)
      max = view.items[i].node_id;
  if (!r->driving) {
    rc = cog_core_drive_begin(cog_cli_app_core(r->app));
    if (rc != COG_OK)
      return rc;
    r->driving = true;
  }
  rc = cog_cli_app_submit(r->app, text, len);
  if (rc == COG_OK) {
    r->busy = true;
    r->baseline = max;
  }
  return rc;
}
cog_result_t cog_tui_runtime_step(cog_tui_runtime_t *r,
                                  cog_tui_runtime_status_t *status) {
  if (!r || !status)
    return COG_ERROR_NULL_PTR;
  if (r->done)
    return COG_ERROR_INVALID_STATE;
  if (!r->busy && !r->shutting) {
    status->done = false;
    status->wait_ms = 20;
    return COG_OK;
  }
  cog_core_step_info_t step = {0};
  cog_result_t rc = cog_core_step(cog_cli_app_core(r->app), &step);
  status->done = step.done;
  status->wait_ms = step.max_wait_ms;
  if (step.done) {
    cog_result_t ended = cog_core_drive_end(cog_cli_app_core(r->app));
    r->driving = false;
    if (rc == COG_OK)
      rc = ended;
    r->busy = false;
    if (r->shutting || rc != COG_OK) {
      r->done = true;
      r->result = rc;
    } else
      status->done = false;
  }
  if (rc == COG_OK && r->busy) {
    cog_cli_display_snapshot_t v;
    rc = cog_cli_app_display_snapshot(r->app, &v);
    if (rc == COG_OK)
      for (size_t i = 0; i < v.item_count; i++)
        if (v.items[i].node_id > r->baseline &&
            v.items[i].role == COG_MESSAGE_ROLE_ASSISTANT)
          r->busy = false;
  }
  return rc;
}
cog_result_t cog_tui_runtime_snapshot(cog_tui_runtime_t *r,
                                      cog_cli_display_snapshot_t *v) {
  if (!r || !v)
    return COG_ERROR_NULL_PTR;
  return cog_cli_app_display_snapshot(r->app, v);
}
cog_result_t cog_tui_runtime_cancel(cog_tui_runtime_t *r) {
  if (!r)
    return COG_ERROR_NULL_PTR;
  if (r->done || r->shutting)
    return COG_ERROR_INVALID_STATE;
  return cog_cli_app_cancel(r->app);
}
cog_result_t cog_tui_runtime_reload(cog_tui_runtime_t *r,
                                    const cog_tui_settings_t *s) {
  if (!r || !s)
    return COG_ERROR_NULL_PTR;
  if (r->done || r->shutting)
    return COG_ERROR_INVALID_STATE;
  cog_result_t rc = cog_tui_settings_validate(s);
  if (rc != COG_OK)
    return rc;
  if (r->mock && (strcmp(s->provider, r->selected.provider) ||
                  strcmp(s->base_url, r->selected.base_url) ||
                  strcmp(s->credential_ref, r->selected.credential_ref)))
    return COG_ERROR_UNSUPPORTED;
  cog_cli_reload_options_t replacement = {
      .model_id = s->model,
      .base_url = r->mock ? NULL : s->base_url,
      .credential_ref = r->mock ? NULL : s->credential_ref};
  uint64_t generation = 0;
  rc = cog_cli_app_reload(r->app, &replacement, &generation);
  if (rc == COG_OK)
    r->selected = *s;
  return rc;
}
cog_result_t cog_tui_runtime_shutdown(cog_tui_runtime_t *r) {
  if (!r)
    return COG_ERROR_NULL_PTR;
  if (r->done || r->shutting)
    return COG_OK;
  cog_result_t rc = COG_OK;
  if (!r->driving) {
    rc = cog_core_drive_begin(cog_cli_app_core(r->app));
    if (rc != COG_OK)
      return rc;
    r->driving = true;
  }
  rc = cog_cli_app_shutdown(r->app);
  if (rc == COG_OK)
    r->shutting = true;
  return rc;
}
cog_result_t cog_tui_runtime_free(cog_tui_runtime_t **handle) {
  if (!handle)
    return COG_ERROR_NULL_PTR;
  cog_tui_runtime_t *r = *handle;
  if (!r)
    return COG_OK;
  if (!r->done)
    return COG_ERROR_INVALID_STATE;
  cog_result_t rc =
      r->driving ? cog_core_drive_end(cog_cli_app_core(r->app)) : COG_OK;
  if (r->result != COG_OK)
    rc = r->result;
  release(r);
  *handle = NULL;
  return rc;
}
