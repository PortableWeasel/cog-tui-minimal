/** @file markdown.h
 * @brief Bounded Markdown-to-styled-rows contract.
 * Owner thread only, non-reentrant. No terminal IO or Cog mutation. The module
 * owns its scratch allocations; returned frames own all text/run storage.
 * See docs/markdown/BLUEPRINT.md and docs/markdown/SCENARIOS.md.
 */
#ifndef COG_TUI_MARKDOWN_H
#define COG_TUI_MARKDOWN_H
#include <cog/result.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct cog_tui_markdown cog_tui_markdown_t;
typedef struct cog_tui_md_frame cog_tui_md_frame_t;
typedef enum cog_tui_md_phase {
    COG_TUI_MD_STREAMING,
    COG_TUI_MD_COMPLETE,
    COG_TUI_MD_CANCELLED,
    COG_TUI_MD_FAILED
} cog_tui_md_phase_t;
typedef enum cog_tui_md_style {
    COG_TUI_MD_BOLD = 1u,
    COG_TUI_MD_ITALIC = 2u,
    COG_TUI_MD_CODE = 4u,
    COG_TUI_MD_DIM = 8u,
    COG_TUI_MD_UNDERLINE = 16u
} cog_tui_md_style_t;
typedef enum cog_tui_md_origin {
    COG_TUI_MD_SOURCE,
    COG_TUI_MD_DECORATION,
    COG_TUI_MD_PREVIEW
} cog_tui_md_origin_t;
typedef enum cog_tui_md_format {
    COG_TUI_MD_FORMAT_MARKDOWN,
    COG_TUI_MD_FORMAT_PLAIN
} cog_tui_md_format_t;
/** @brief Bounded virtual viewport; layout may count rows without storing them. */
typedef struct cog_tui_md_window {
    size_t columns; /**< 1..1000 display cells. */
    size_t first_row; /**< Zero-based logical row; SIZE_MAX selects the tail. */
    size_t row_count; /**< Requested visible rows, 1..300. */
} cog_tui_md_window_t;
/** @brief Zero fields select documented defaults; these bound UI-owned data.
 * They do not constitute an allocator hook or a hard memory cap inside MD4C.
 */
typedef struct cog_tui_md_limits {
    size_t source_bytes; /**< Default 131072 per Markdown parse. */
    size_t frame_bytes; /**< Default 4194304 (text + metadata). */
    size_t rows; /**< Default 16384 logical Markdown rows; PLAIN bypasses this. */
    size_t nesting; /**< Default 64 supported nested blocks/spans. */
    size_t table_columns; /**< Default 32. */
} cog_tui_md_limits_t;
/** @brief A complete authoritative message prefix, borrowed during render only.
 * message_key is assigned by UI reconciliation, nonzero, stable across active
 * node_id=0 to committed node_id promotion; never use array position as identity.
 */
typedef struct cog_tui_md_source {
    const char *bytes; /**< NULL iff byte_count=0; embedded NUL is escaped. */
    size_t byte_count;
    uint64_t message_key;
    cog_tui_md_phase_t phase;
} cog_tui_md_source_t;
/** @brief One sanitized, single-row UTF-8 text run. No ESC/C0/C1 controls.
 * Bytes need not be NUL-terminated. Newlines/tabs have already become layout.
 * A run cannot split a supported display cluster or overrun its row.
 */
typedef struct cog_tui_md_run {
    const char *text;
    size_t bytes;
    size_t column, cells;
    unsigned style; /**< Bitwise cog_tui_md_style_t, no backend attributes. */
    cog_tui_md_origin_t origin;
} cog_tui_md_run_t;
typedef struct cog_tui_md_row {
    const cog_tui_md_run_t *runs;
    size_t run_count;
    bool provisional; /**< View may change when the prefix grows. */
} cog_tui_md_row_t;
typedef struct cog_tui_md_view {
    const cog_tui_md_row_t *rows;
    size_t row_count, columns;
    uint64_t message_key;
    cog_tui_md_phase_t phase;
    size_t first_row, total_rows; /**< Visible slice and full logical row count. */
    size_t pending_utf8_bytes; /**< 0..3 held incomplete UTF-8 suffix bytes. */
    bool preview_applied; /**< Speculative suffix/decorations used this frame. */
} cog_tui_md_view_t;

/** @brief Construct owner-thread scratch context, no terminal/global mutation.
 * @param[out] out Required, set NULL on failure, owned handle on success.
 * @param[in] limits Optional, copied; zero fields select defaults above.
 * @return OK, NULL_PTR, INVALID_ARG for unrepresentable limits, or NOMEM.
 */
cog_result_t cog_tui_markdown_new(cog_tui_markdown_t **out,
    const cog_tui_md_limits_t *limits);
/** @brief Render whole prefix at width; includes reversible EOF preview only
 * during STREAMING. Terminal phases use exact source, no speculative closures.
 * @param[in,out] markdown Live owner.
 * @param[in] source Required descriptor, borrowed only during call.
 * @param[in] format MARKDOWN, or PLAIN bypassing parsing/synthetic completion.
 * @param[in] window Required bounded viewport; first_row beyond end is empty.
 * @param[out] out Required, set NULL on failure; caller owns successful frame.
 * @return OK; NULL_PTR; INVALID_ARG for window/format/phase/key/pointer mismatch;
 * LIMIT for configured bound; NOMEM; PROTOCOL for parser/adapter failure.
 * @note source_bytes limit applies to Markdown parsing only; PLAIN can scan a
 * larger borrowed snapshot, storing only requested rows within frame limits.
 * PLAIN bypasses Markdown logical-row/nesting/table bounds, not frame storage or
 * viewport bounds. Logical totals use checked size_t arithmetic in either mode.
 * Incomplete UTF-8 suffix is held with pending_utf8_bytes while STREAMING;
 * terminal phases visibly replace invalid sequences, never discard source.
 * Caller must establish a UTF-8 LC_CTYPE locale for wide/combining characters.
 * Unsupported format/emoji clusters use visible escapes. Table widths divide
 * available space equally; narrow tables use vertical header/value records.
 * Ambiguous overflow tables return PROTOCOL so callers can request PLAIN.
 * Entities are displayed literally, preserving their exact received spelling.
 * No hidden state keyed to chunk boundaries: same prefix/phase/window gives
 * same frame (given identical locale/capability policy). No previously returned
 * frame is invalidated. Synthetic closure
 * bytes never appear as authored text. All arithmetic checked before allocation.
 */
cog_result_t cog_tui_markdown_render(cog_tui_markdown_t *markdown,
    const cog_tui_md_source_t *source, cog_tui_md_format_t format,
    const cog_tui_md_window_t *window, cog_tui_md_frame_t **out);
/** @brief Borrow immutable rows until frame_free; allocation/IO free.
 * @param[in] frame Required owned frame. @param[out] view Required destination.
 * @return OK or NULL_PTR. Error leaves view unchanged.
 */
cog_result_t cog_tui_md_frame_view(const cog_tui_md_frame_t *frame,
    cog_tui_md_view_t *view);
/** @brief Query immutable frame-owned allocation bytes for cache accounting.
 * @param[in] frame Required live frame. @param[out] bytes Required destination.
 * @return OK or NULL_PTR; error leaves bytes unchanged. No allocation or IO.
 * @note Includes the frame, all row/run capacities and owned text bytes;
 * excludes allocator bookkeeping, caller data and temporary parsing storage.
 * The result is stable for the frame lifetime and never exceeds frame_bytes.
 */
cog_result_t cog_tui_md_frame_storage(const cog_tui_md_frame_t *frame,
    size_t *bytes);
/** @brief Release one frame, set handle NULL; NULL handle is an allowed no-op.
 * @param[in,out] frame Required pointer. @return OK or NULL_PTR.
 */
cog_result_t cog_tui_md_frame_free(cog_tui_md_frame_t **frame);
/** @brief Release scratch owner; independent frames remain valid.
 * @param[in,out] markdown Required pointer; NULL handle is a no-op.
 * @return OK or NULL_PTR. No callbacks or IO.
 */
cog_result_t cog_tui_markdown_free(cog_tui_markdown_t **markdown);
#ifdef __cplusplus
}
#endif
#endif
