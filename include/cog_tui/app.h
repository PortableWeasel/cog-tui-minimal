/** @file app.h
 * @brief Single-instance terminal application entry. Owner thread only.
 * Readline's callback API requires one temporary process-global binding to the
 * caller-owned application context. This is the sole UI mutable-global bridge;
 * reject a second instance, clear the binding on every exit. Signal handlers
 * only record sig_atomic_t flags; ordinary loop performs all library calls.
 */
#ifndef COG_TUI_APP_H
#define COG_TUI_APP_H
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Run terminal setup and conversation until exit or fatal failure.
 * @param[in] argc Argument count, at least1.
 * @param[in] argv Required argument vector; borrowed until return.
 * @return 0 on normal exit/help; nonzero on usage, setup, runtime or cleanup error.
 * @pre Interactive execution requires a terminal; explicit help may run anywhere.
 * @post Readline removed, temporary callback binding cleared, terminal modes,
 * signal dispositions and redirected descriptors restored, owned runtime freed
 * after drain. No signal handler calls readline or Cog mutation APIs.
 * @note Saved preferences are loaded from an application-owned private directory;
 * prompt text and credentials are never persisted as readline history. Rendering
 * escapes terminal controls in untrusted Cog text. Keep streaming progress while
 * editing and preserve draft/cursor across redraw. No implicit prompt replay.
 * ANSI/VT-compatible terminal required. Output scrolls above a separator and
 * a single-row, horizontally scrolling readline prompt. Each assistant message
 * has one role label and a Markdown frame from its exact current prefix.
 * Ctrl-P/Ctrl-N page the viewport; Ctrl-T resumes live tail without draft edits.
 * Readline auxiliary output is suppressed; the screen is the sole painter.
 * Resize repaints the public snapshot, preserving the input draft and cursor.
 * Restore the full terminal scrolling region before returning.
 */
int cog_tui_app_run(int argc, char **argv);
#ifdef __cplusplus
}
#endif
#endif
