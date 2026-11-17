/** @file transcript.h
 * @brief Owner-thread snapshot identity, bounded frame cache and viewport.
 * Uses only public Cog snapshots. Never retains borrowed snapshot pointers.
 * No terminal IO or runtime mutation. Returned scene borrows end at the next
 * sync/compose/fail/free. Caller establishes the renderer's UTF-8 locale.
 */
#ifndef COG_TUI_TRANSCRIPT_H
#define COG_TUI_TRANSCRIPT_H
#include <cog_cli/cli.h>
#include <cog_tui/screen.h>
typedef struct cog_tui_transcript cog_tui_transcript_t;
/** @brief Construct with borrowed renderer, valid until free. Cache budget is
 * 4 MiB incl owned metadata/source/frame allocations, excluding allocator
 * bookkeeping. Visible uncached frames have an independent 4 MiB aggregate cap.
 * @return OK, NULL_PTR, NOMEM. Required out set NULL on error. */
cog_result_t cog_tui_transcript_new(cog_tui_transcript_t **out,
                                    cog_tui_markdown_t *markdown);
/** @brief Reconcile a full public snapshot, preserving durable identities and
 * unique active-to-committed promotion within the same user-turn predecessor.
 * @param transcript Required owner. @param snapshot Required call-only borrow.
 * @param changed Required output; true on content/phase/order changes, or an
 * uncached source that cannot be compared exactly. Session changes are
 * rejected.
 * @return OK, NULL_PTR, INVALID_ARG, LIMIT (>4096 visible records), PROTOCOL
 * (session/duplicate identity), OVERFLOW. Error requires shutdown/free;
 * metadata may have advanced but no source ownership is lost. No renderer
 * allocation.
 */
cog_result_t cog_tui_transcript_sync(cog_tui_transcript_t *transcript,
                                     const cog_cli_display_snapshot_t *snapshot,
                                     bool *changed);
/** @brief Compose exact current synchronized snapshot at bounded geometry.
 * @param rows 1..300. @param columns 1..1000. @param scene Required output.
 * @return OK, NULL_PTR, INVALID_ARG, INVALID_STATE, LIMIT, NOMEM, PROTOCOL.
 * Render LIMIT/PROTOCOL automatically retries exact-source PLAIN. Status labels
 * plain fallback/paused/expired anchor. No frame changes after return until
 * next call. On failure caller may restore terminal and exit; old scene not
 * promised.
 */
cog_result_t
cog_tui_transcript_compose(cog_tui_transcript_t *transcript,
                           const cog_cli_display_snapshot_t *snapshot,
                           size_t rows, size_t columns,
                           cog_tui_screen_scene_t *scene);
/** @brief Request one page backward(-1), forward(1), or live tail(0).
 * No draft mutation; applied on next compose. Paused anchor uses stable key and
 * logical row, clamped after reflow. @return OK, NULL_PTR, INVALID_ARG. */
cog_result_t cog_tui_transcript_scroll(cog_tui_transcript_t *transcript,
                                       int direction);
/** @brief Mark a stopped runtime's remaining active prefix FAILED. No source
 * bytes are invented. Next sync/compose removes provisional rendering. Only
 * for terminal runtime failure, never a cancellation request. Sticky until
 * free.
 * @return OK or NULL_PTR. */
cog_result_t cog_tui_transcript_fail(cog_tui_transcript_t *transcript);
/** @brief Inspect synchronized item identity for tests/compositor coordination.
 * index is visible user/assistant order. Required outputs unchanged on error.
 * @return OK, NULL_PTR, NOT_FOUND. */
cog_result_t cog_tui_transcript_identity(const cog_tui_transcript_t *transcript,
                                         size_t index, uint64_t *key,
                                         cog_tui_md_phase_t *phase);
/** @brief Free frames and cache; borrowed renderer remains live.
 * Required handle pointer; NULL handle no-op. @return OK or NULL_PTR. */
cog_result_t cog_tui_transcript_free(cog_tui_transcript_t **transcript);
#endif
