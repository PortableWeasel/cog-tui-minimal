/** @file screen.h
 * @brief DRAFT: one terminal owner, backend-neutral Markdown frames + input.
 * Not a shipped API. This screen owns terminal drawing/modes. Readline keeps
 * editing/keymaps; its hooks update a copied input view and emit no terminal IO.
 * Owner thread only; callbacks route through the existing single-instance
 * readline bridge. Restore every replaced readline hook on close.
 */
#ifndef COG_TUI_SCREEN_H
#define COG_TUI_SCREEN_H
#include "markdown.h"
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct cog_tui_screen cog_tui_screen_t;
typedef enum cog_tui_screen_backend {
    COG_TUI_SCREEN_ANSI,
    COG_TUI_SCREEN_NCURSESW
} cog_tui_screen_backend_t;
typedef struct cog_tui_screen_input {
    const char *label; /**< Required safe UTF-8 prompt label. */
    const char *bytes; /**< Readline draft, NULL iff byte_count=0. */
    size_t byte_count, cursor_byte; /**< <=1048576 bytes; cursor<=byte_count. */
} cog_tui_screen_input_t;
/** @brief Total physical dimensions; usable output is computed by screen. */
typedef struct cog_tui_screen_size {
    size_t rows, columns;
    size_t output_rows, output_columns; /**< Excludes input and status rows. */
} cog_tui_screen_size_t;
typedef struct cog_tui_screen_slice {
    const cog_tui_md_view_t *view; /**< Borrowed visible rows from one message. */
    const char *label; /**< Optional safe role label on its own row. */
} cog_tui_screen_slice_t;
typedef struct cog_tui_screen_scene {
    const cog_tui_screen_slice_t *slices; /**< Ordered, NULL iff slice_count=0. */
    size_t slice_count;
    const char *status; /**< Optional plain status, <=512 bytes; no Markdown. */
} cog_tui_screen_scene_t;

/** @brief Initialize selected screen on explicit caller-owned terminal streams.
 * @param[out] out Required, NULL on failure, owned handle on success.
 * @param[in] backend Supported backend, fixed for handle lifetime.
 * @param[in] input Required terminal stream, borrowed until close.
 * @param[in] output Required saved UI stream, borrowed until close; no stderr.
 * @return OK; NULL_PTR; INVALID_ARG; UNSUPPORTED for absent backend/capability;
 * IO; NOMEM. Failure unwinds modes/hooks without closing supplied streams.
 */
cog_result_t cog_tui_screen_open(cog_tui_screen_t **out,
    cog_tui_screen_backend_t backend, FILE *input, FILE *output);
/** @brief Query/resynchronize terminal geometry without signal-handler work.
 * @param[in,out] screen Live owner. @param[out] size Required result.
 * @return OK, NULL_PTR, IO. No Cog calls; caller reflows at output width.
 */
cog_result_t cog_tui_screen_resize(cog_tui_screen_t *screen,
    cog_tui_screen_size_t *size);
/** @brief Stage one copied input view; preserve byte-indexed readline cursor.
 * @param[in,out] screen Live owner. @param[in] input Required view, call borrow.
 * @return OK, NULL_PTR, INVALID_ARG, LIMIT or NOMEM; error preserves old input.
 * @note label <=128 bytes; malformed/control bytes render visibly escaped.
 * Cursor inside an encoding/display cluster maps to a preceding safe boundary;
 * never mutate readline's byte buffer or its cursor to achieve that display.
 */
cog_result_t cog_tui_screen_input(cog_tui_screen_t *screen,
    const cog_tui_screen_input_t *input);
/** @brief Paint viewport, separator and staged input in one screen transaction.
 * @param[in,out] screen Live owner. @param[in] scene Required visible slices.
 * @return OK, NULL_PTR, INVALID_ARG, AGAIN, LIMIT or IO. Call-only borrows.
 * @note Sum of slice rows and label rows must fit output_rows; each view's
 * columns equals output_columns. Geometry mismatch returns AGAIN, requesting
 * resize/reflow; INVALID_ARG covers invalid slices. Empty geometry displays a
 * compact resize notice without writing outside the terminal. Scene borrows
 * end on return. Reset attributes between roles and before input. Never retain frame
 * pointers or write Markdown text as terminal commands/printf format strings.
 * IO may leave physical cells partially updated: retry as a full repaint or
 * close; neither input nor frame ownership is consumed by this call.
 */
cog_result_t cog_tui_screen_present(cog_tui_screen_t *screen,
    const cog_tui_screen_scene_t *scene);
/** @brief Restore modes/hooks/screen state and consume handle on every outcome.
 * @param[in,out] screen Required pointer; NULL handle is a no-op.
 * @return OK, NULL_PTR or IO. Caller remains responsible for stream closure.
 */
cog_result_t cog_tui_screen_close(cog_tui_screen_t **screen);
#ifdef __cplusplus
}
#endif
#endif
