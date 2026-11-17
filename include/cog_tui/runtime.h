/** @file runtime.h
 * @brief Owner-thread adapter to installed public Cog app and core APIs.
 * No calls are signal-safe. Settings are copied on construction/reload.
 * Snapshot borrows end at the next mutating runtime call. No private Cog API.
 */
#ifndef COG_TUI_RUNTIME_H
#define COG_TUI_RUNTIME_H
#include <cog_tui/settings.h>
#include <cog_cli/cli.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Explicit constructor-owned runtime; never copy a live handle. */
typedef struct cog_tui_runtime cog_tui_runtime_t;
/** @brief Constructor selection. */
typedef struct cog_tui_runtime_options {
    bool mock; /**< Explicit deterministic test mode; no real provider traffic. */
    const char *config_path; /**< Required path to UI-owned empty Cog config. */
    const char *log_path; /**< Optional UI-owned diagnostic path. */
} cog_tui_runtime_options_t;
/** @brief One cooperative service result. */
typedef struct cog_tui_runtime_status {
    bool done; /**< Driver stopped; no new work admitted. */
    uint32_t wait_ms; /**< Maximum wait before next service slice. */
} cog_tui_runtime_status_t;
/** @brief Construct app/net and begin cooperative driving, without submitting.
 * @param[out] out Required; set NULL on failure, owned handle on success.
 * @param[in] settings Complete validated selection, copied.
 * @param[in] options Required options; strings copied/consumed before return.
 * @return OK; NULL_PTR; validation or Cog/dependency constructor errors.
 * @note REAL initializes and owns curl runtime then net, then app; reverse
 * cleanup. Only one real runtime may own dependency lifecycle in this process.
 * Persistence and tools disabled. Explicit fields override ambient Cog defaults.
 * Cog may write stdout/stderr: application owns output routing before new().
 */
cog_result_t cog_tui_runtime_new(cog_tui_runtime_t **out,
    const cog_tui_settings_t *settings, const cog_tui_runtime_options_t *options);
/** @brief Submit one explicit prompt, copied by Cog on success.
 * @param[in,out] runtime Live non-shutting-down owner.
 * @param[in] text Nonempty UTF-8 prompt, no embedded NUL.
 * @param[in] length Exact byte length.
 * @return OK means queued only; NULL_PTR; INVALID_ARG; INVALID_STATE; or Cog
 * capacity/admission error. Never automatically retry uncertain submissions.
 */
cog_result_t cog_tui_runtime_submit(cog_tui_runtime_t *runtime,
    const char *text, size_t length);
/** @brief Service one bounded nonblocking core slice.
 * @param[in,out] runtime Live owner.
 * @param[out] status Required result; populated on valid-work errors too.
 * @return OK; NULL_PTR; INVALID_STATE after done; Cog service error.
 * @note Driver error stops new admission; retain handle for explicit cleanup.
 */
cog_result_t cog_tui_runtime_step(cog_tui_runtime_t *runtime,
    cog_tui_runtime_status_t *status);
/** @brief Borrow full current display state.
 * @param[in,out] runtime Live owner. @param[out] snapshot Required destination.
 * @return OK; NULL_PTR; or Cog snapshot error. Borrow expires at mutation/free.
 */
cog_result_t cog_tui_runtime_snapshot(cog_tui_runtime_t *runtime,
    cog_cli_display_snapshot_t *snapshot);
/** @brief Request cancellation; success means queued, not completed.
 * @param[in,out] runtime Live owner.
 * @return OK; NULL_PTR; INVALID_STATE; Cog queue error. Continue stepping.
 */
cog_result_t cog_tui_runtime_cancel(cog_tui_runtime_t *runtime);
/** @brief Publish a selection for future turns; preserve current conversation.
 * @param[in,out] runtime Live owner.
 * @param[in] settings Complete validated new selection; copied on success.
 * @return OK after publication only; NULL_PTR; validation/Cog reload error.
 * @note UI labels selection pending/for next turn. Publication is not activation.
 * MOCK accepts model only; other fields must equal prior values. No save here.
 */
cog_result_t cog_tui_runtime_reload(cog_tui_runtime_t *runtime,
    const cog_tui_settings_t *settings);
/** @brief Enqueue graceful shutdown, then caller continues step until done.
 * @param[in,out] runtime Live owner; repeated successful request is idempotent.
 * @return OK; NULL_PTR; or Cog queue error. Failure permits retry of shutdown.
 */
cog_result_t cog_tui_runtime_shutdown(cog_tui_runtime_t *runtime);
/** @brief Consume a fully stopped runtime and release app/net/dependencies.
 * @param[in,out] runtime Required pointer; NULL handle is successful no-op.
 * @return OK; NULL_PTR; INVALID_STATE if driver not done (handle retained);
 * otherwise cleanup/latched driver result with handle set NULL.
 * @pre No active tool execution (tools disabled in this profile).
 */
cog_result_t cog_tui_runtime_free(cog_tui_runtime_t **runtime);
#ifdef __cplusplus
}
#endif
#endif
