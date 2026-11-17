#include <cog_tui/transcript.h>
#include <stdlib.h>
#include <string.h>
#define RECORDS 4096u
#define BUDGET (4u * 1024u * 1024u)
typedef struct record {
  uint64_t key, node, predecessor, used;
  cog_message_role_t role;
  cog_tui_md_phase_t phase;
  size_t item_index;
  bool seen, selected, plain;
  char *source;
  size_t length, charge;
  cog_tui_md_frame_t *frame;
  cog_tui_md_window_t window;
} record_t;
struct cog_tui_transcript {
  cog_tui_markdown_t *markdown;
  record_t records[RECORDS];
  size_t order[RECORDS], count, cache_bytes;
  uint64_t next_key, clock, generation, sequence;
  size_t snapshot_count;
  char session[128];
  cog_tui_md_view_t views[300];
  cog_tui_screen_slice_t slices[300];
  cog_tui_md_frame_t *temporary[300];
  size_t temporary_count, temporary_bytes;
  bool paused, expired, fallback, failed;
  int direction;
  uint64_t anchor;
  size_t anchor_row;
};
static void uncache(cog_tui_transcript_t *t, record_t *r) {
  (void)cog_tui_md_frame_free(&r->frame);
  free(r->source);
  r->source = NULL;
  t->cache_bytes -= r->charge;
  r->charge = 0;
}
static void clear_scene(cog_tui_transcript_t *t) {
  for (size_t i = 0; i < t->temporary_count; ++i)
    (void)cog_tui_md_frame_free(&t->temporary[i]);
  t->temporary_count = 0;
  t->temporary_bytes = 0;
  for (size_t i = 0; i < RECORDS; ++i)
    t->records[i].selected = false;
}
static cog_tui_md_phase_t phase(const cog_cli_display_item_t *item) {
  if (item->role == COG_MESSAGE_ROLE_USER)
    return COG_TUI_MD_COMPLETE;
  if (!item->node_id)
    return COG_TUI_MD_STREAMING;
  if (item->status == COG_MESSAGE_COMPLETE)
    return COG_TUI_MD_COMPLETE;
  return item->status == COG_MESSAGE_CANCELLED ? COG_TUI_MD_CANCELLED
                                               : COG_TUI_MD_FAILED;
}
cog_result_t cog_tui_transcript_new(cog_tui_transcript_t **out,
                                    cog_tui_markdown_t *markdown) {
  if (!out)
    return COG_ERROR_NULL_PTR;
  *out = NULL;
  if (!markdown)
    return COG_ERROR_NULL_PTR;
  cog_tui_transcript_t *t = calloc(1, sizeof(*t));
  if (!t)
    return COG_ERROR_NOMEM;
  t->markdown = markdown;
  t->next_key = 1;
  t->cache_bytes = sizeof(*t);
  *out = t;
  return COG_OK;
}
cog_result_t cog_tui_transcript_sync(cog_tui_transcript_t *t,
                                     const cog_cli_display_snapshot_t *snapshot,
                                     bool *changed) {
  if (!t || !snapshot || !changed || !snapshot->session_id)
    return COG_ERROR_NULL_PTR;
  if ((!snapshot->items && snapshot->item_count) || !snapshot->generation)
    return COG_ERROR_INVALID_ARG;
  size_t len = 0;
  while (len < 128 && snapshot->session_id[len])
    ++len;
  if (!len || len == 128)
    return COG_ERROR_PROTOCOL;
  if (t->session[0] && strcmp(t->session, snapshot->session_id))
    return COG_ERROR_PROTOCOL;
  clear_scene(t);
  bool dirty = false;
  if (!t->session[0])
    memcpy(t->session, snapshot->session_id, len + 1);
  for (size_t j = 0; j < RECORDS; ++j)
    t->records[j].seen = false;
  size_t count = 0;
  uint64_t predecessor = 0;
  for (size_t i = 0; i < snapshot->item_count; ++i) {
    const cog_cli_display_item_t *item = &snapshot->items[i];
    if (item->node_id) {
      for (size_t j = 0; j < i; ++j)
        if (snapshot->items[j].node_id == item->node_id)
          return COG_ERROR_PROTOCOL;
    }
    if (item->role != COG_MESSAGE_ROLE_USER &&
        item->role != COG_MESSAGE_ROLE_ASSISTANT)
      continue;
    if ((!item->text && item->text_len) || count == RECORDS)
      return COG_ERROR_LIMIT;
    if (item->role == COG_MESSAGE_ROLE_USER)
      predecessor = item->node_id;
    size_t found = RECORDS;
    for (size_t j = 0; j < RECORDS; ++j) {
      record_t *r = &t->records[j];
      if (r->key && r->role == item->role &&
          ((item->node_id && r->node == item->node_id) ||
           (!item->node_id && !r->node && r->predecessor == predecessor))) {
        if (r->seen)
          return COG_ERROR_PROTOCOL;
        found = j;
        break;
      }
    }
    if (found == RECORDS && item->node_id &&
        item->role == COG_MESSAGE_ROLE_ASSISTANT) {
      for (size_t j = 0; j < RECORDS; ++j) {
        record_t *r = &t->records[j];
        if (r->key && !r->node && !r->seen && r->role == item->role &&
            r->predecessor == predecessor) {
          if (found != RECORDS)
            return COG_ERROR_PROTOCOL;
          found = j;
        }
      }
    }
    if (found == RECORDS) {
      for (size_t j = 0; j < RECORDS; ++j)
        if (!t->records[j].key) {
          found = j;
          break;
        }
      if (found == RECORDS) {
        /* A full snapshot may replace a record without growing the set. */
        for (size_t j = 0; j < RECORDS; ++j) {
          record_t *old = &t->records[j];
          if (old->seen || !old->node)
            continue;
          bool present = false;
          for (size_t k = 0; k < snapshot->item_count; ++k)
            if (snapshot->items[k].node_id == old->node) {
              present = true;
              break;
            }
          if (!present) {
            uncache(t, old);
            memset(old, 0, sizeof(*old));
            found = j;
            break;
          }
        }
      }
      if (found == RECORDS)
        return COG_ERROR_LIMIT;
      if (t->next_key == UINT64_MAX)
        return COG_ERROR_OVERFLOW;
      t->records[found].key = t->next_key++;
      dirty = true;
    }
    record_t *r = &t->records[found];
    cog_tui_md_phase_t p = phase(item);
    if (t->failed && !item->node_id)
      p = COG_TUI_MD_FAILED;
    if (!r->frame || r->length != item->text_len || r->phase != p ||
        (item->text_len && memcmp(r->source, item->text, item->text_len))) {
      uncache(t, r);
      dirty = true;
    }
    r->seen = true;
    r->node = item->node_id;
    r->role = item->role;
    r->phase = p;
    r->predecessor = predecessor;
    r->item_index = i;
    if (count >= t->count || t->order[count] != found)
      dirty = true;
    t->order[count++] = found;
  }
  for (size_t j = 0; j < RECORDS; ++j)
    if (t->records[j].key && !t->records[j].seen) {
      uncache(t, &t->records[j]);
      memset(&t->records[j], 0, sizeof(record_t));
      dirty = true;
    }
  if (count != t->count)
    dirty = true;
  t->count = count;
  t->generation = snapshot->generation;
  t->sequence = snapshot->through_sequence;
  t->snapshot_count = snapshot->item_count;
  *changed = dirty;
  return COG_OK;
}
static bool reserve(cog_tui_transcript_t *t, size_t charge, record_t *current) {
  if (charge > BUDGET - sizeof(*t))
    return false;
  while (t->cache_bytes > BUDGET - charge) {
    record_t *old = NULL;
    for (size_t j = 0; j < RECORDS; ++j) {
      record_t *r = &t->records[j];
      if (r != current && r->frame && !r->selected &&
          (!old || r->used < old->used))
        old = r;
    }
    if (!old)
      return false;
    uncache(t, old);
  }
  return true;
}
static cog_result_t frame(cog_tui_transcript_t *t, record_t *r,
                          const cog_cli_display_snapshot_t *snapshot,
                          size_t columns, size_t rows, size_t first,
                          cog_tui_md_view_t *view, bool select) {
  cog_tui_md_window_t window = {columns, first, rows};
  if (r->frame && r->window.columns == columns &&
      r->window.first_row == first && r->window.row_count == rows) {
    r->used = ++t->clock;
    r->selected = select;
    t->fallback |= r->plain;
    return cog_tui_md_frame_view(r->frame, view);
  }
  if (r->selected)
    return COG_ERROR_INVALID_STATE;
  uncache(t, r);
  const cog_cli_display_item_t *item = &snapshot->items[r->item_index];
  cog_tui_md_source_t source = {item->text, item->text_len, r->key, r->phase};
  cog_tui_md_frame_t *f = NULL;
  cog_tui_md_format_t format = r->role == COG_MESSAGE_ROLE_USER
                                   ? COG_TUI_MD_FORMAT_PLAIN
                                   : COG_TUI_MD_FORMAT_MARKDOWN;
  cog_result_t rc =
      cog_tui_markdown_render(t->markdown, &source, format, &window, &f);
  r->plain = false;
  if (format == COG_TUI_MD_FORMAT_MARKDOWN &&
      (rc == COG_ERROR_LIMIT || rc == COG_ERROR_PROTOCOL)) {
    rc = cog_tui_markdown_render(t->markdown, &source, COG_TUI_MD_FORMAT_PLAIN,
                                 &window, &f);
    r->plain = true;
  }
  if (rc != COG_OK)
    return rc;
  size_t storage = 0;
  (void)cog_tui_md_frame_storage(f, &storage);
  if (item->text_len == SIZE_MAX || storage > SIZE_MAX - item->text_len - 1) {
    (void)cog_tui_md_frame_free(&f);
    return COG_ERROR_LIMIT;
  }
  size_t charge = storage + item->text_len + 1;
  if (reserve(t, charge, r)) {
    char *copy = malloc(item->text_len + 1);
    if (copy) {
      if (item->text_len)
        memcpy(copy, item->text, item->text_len);
      copy[item->text_len] = '\0';
      r->source = copy;
      r->length = item->text_len;
      r->frame = f;
      r->window = window;
      r->charge = charge;
      t->cache_bytes += charge;
      r->used = ++t->clock;
      r->selected = select;
    }
  }
  if (!r->frame) {
    if (t->temporary_count == 300 || storage > BUDGET - t->temporary_bytes) {
      (void)cog_tui_md_frame_free(&f);
      return COG_ERROR_LIMIT;
    }
    t->temporary[t->temporary_count++] = f;
    t->temporary_bytes += storage;
  }
  t->fallback |= r->plain;
  return cog_tui_md_frame_view(f, view);
}
cog_result_t cog_tui_transcript_compose(
    cog_tui_transcript_t *t, const cog_cli_display_snapshot_t *snapshot,
    size_t rows, size_t columns, cog_tui_screen_scene_t *scene) {
  if (!t || !snapshot || !scene)
    return COG_ERROR_NULL_PTR;
  if (!rows || rows > 300 || !columns || columns > 1000)
    return COG_ERROR_INVALID_ARG;
  if (!snapshot->session_id || strcmp(t->session, snapshot->session_id) ||
      snapshot->generation != t->generation ||
      snapshot->through_sequence != t->sequence ||
      snapshot->item_count != t->snapshot_count)
    return COG_ERROR_INVALID_STATE;
  clear_scene(t);
  t->fallback = false;
  size_t n = 0, remaining = rows;
  size_t anchor_index = 0;
  if (t->paused && t->count) {
    bool found = false;
    for (size_t i = 0; i < t->count; ++i)
      if (t->records[t->order[i]].key == t->anchor) {
        anchor_index = i;
        found = true;
        break;
      }
    if (!found) {
      t->expired = true;
      t->anchor = t->records[t->order[0]].key;
      t->anchor_row = 0;
    }
    if (t->direction) {
      cog_tui_md_view_t measure;
      record_t *r = &t->records[t->order[anchor_index]];
      cog_result_t rc = frame(t, r, snapshot, columns, rows > 1 ? rows - 1 : 1,
                              SIZE_MAX, &measure, false);
      if (rc != COG_OK)
        return rc;
      size_t page = rows > 1 ? rows - 1 : 1;
      if (t->direction < 0) {
        if (t->anchor_row >= page)
          t->anchor_row -= page;
        else if (anchor_index) {
          --anchor_index;
          r = &t->records[t->order[anchor_index]];
          rc = frame(t, r, snapshot, columns, page, SIZE_MAX, &measure, false);
          if (rc != COG_OK)
            return rc;
          t->anchor_row = measure.first_row;
        } else
          t->anchor_row = 0;
      } else if (t->anchor_row < measure.total_rows &&
                 page < measure.total_rows - t->anchor_row)
        t->anchor_row += page;
      else if (anchor_index + 1 < t->count) {
        ++anchor_index;
        t->anchor_row = 0;
      }
      t->anchor = t->records[t->order[anchor_index]].key;
    }
    for (size_t i = anchor_index; i < t->count && remaining > 1; ++i) {
      record_t *r = &t->records[t->order[i]];
      size_t first = i == anchor_index ? t->anchor_row : 0;
      cog_result_t rc = frame(t, r, snapshot, columns, remaining - 1, first,
                              &t->views[n], true);
      if (rc != COG_OK)
        return rc;
      if (first && first >= t->views[n].total_rows) {
        r->selected = false;
        t->anchor_row = 0;
        rc = frame(t, r, snapshot, columns, remaining - 1, 0, &t->views[n],
                   true);
        if (rc != COG_OK)
          return rc;
      }
      t->slices[n] = (cog_tui_screen_slice_t){
          &t->views[n],
          r->role == COG_MESSAGE_ROLE_USER ? "you:" : "assistant:"};
      remaining -= t->views[n].row_count + 1;
      ++n;
    }
  } else {
    for (size_t i = t->count; i > 0 && remaining > 1; --i) {
      record_t *r = &t->records[t->order[i - 1]];
      cog_result_t rc = frame(t, r, snapshot, columns, remaining - 1, SIZE_MAX,
                              &t->views[n], true);
      if (rc != COG_OK)
        return rc;
      t->slices[n] = (cog_tui_screen_slice_t){
          &t->views[n],
          r->role == COG_MESSAGE_ROLE_USER ? "you:" : "assistant:"};
      remaining -= t->views[n].row_count + 1;
      t->anchor = r->key;
      t->anchor_row = t->views[n].first_row;
      ++n;
    }
    for (size_t i = 0; i < n / 2; ++i) {
      cog_tui_screen_slice_t swap = t->slices[i];
      t->slices[i] = t->slices[n - i - 1];
      t->slices[n - i - 1] = swap;
    }
  }
  t->direction = 0;
  const char *status =
      t->paused
          ? (t->expired ? "Scroll paused; anchor expired. Ctrl-T: live"
                        : "Scroll paused. Ctrl-P/Ctrl-N; Ctrl-T: live")
          : (t->fallback ? "Plain view: Markdown limit or unsupported syntax"
                         : "--- input ---");
  *scene = (cog_tui_screen_scene_t){t->slices, n, status};
  return COG_OK;
}
cog_result_t cog_tui_transcript_scroll(cog_tui_transcript_t *t, int direction) {
  if (!t)
    return COG_ERROR_NULL_PTR;
  if (direction < -1 || direction > 1)
    return COG_ERROR_INVALID_ARG;
  t->paused = direction != 0;
  t->direction = direction;
  if (!direction)
    t->expired = false;
  return COG_OK;
}
cog_result_t cog_tui_transcript_fail(cog_tui_transcript_t *t) {
  if (!t)
    return COG_ERROR_NULL_PTR;
  t->failed = true;
  for (size_t i = 0; i < RECORDS; ++i)
    if (t->records[i].key && !t->records[i].node) {
      uncache(t, &t->records[i]);
      t->records[i].phase = COG_TUI_MD_FAILED;
    }
  return COG_OK;
}
cog_result_t cog_tui_transcript_identity(const cog_tui_transcript_t *t,
                                         size_t index, uint64_t *key,
                                         cog_tui_md_phase_t *p) {
  if (!t || !key || !p)
    return COG_ERROR_NULL_PTR;
  if (index >= t->count)
    return COG_ERROR_NOT_FOUND;
  *key = t->records[t->order[index]].key;
  *p = t->records[t->order[index]].phase;
  return COG_OK;
}
cog_result_t cog_tui_transcript_free(cog_tui_transcript_t **transcript) {
  if (!transcript)
    return COG_ERROR_NULL_PTR;
  if (!*transcript)
    return COG_OK;
  cog_tui_transcript_t *t = *transcript;
  clear_scene(t);
  for (size_t i = 0; i < RECORDS; ++i)
    uncache(t, &t->records[i]);
  free(t);
  *transcript = NULL;
  return COG_OK;
}
