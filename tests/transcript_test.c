#include <assert.h>
#include <cog_tui/transcript.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
static void content(const cog_tui_screen_scene_t *scene, char *out,
                    size_t capacity) {
  size_t used = 0;
  for (size_t i = 0; i < scene->slice_count; ++i) {
    const cog_tui_md_view_t *v = scene->slices[i].view;
    for (size_t j = 0; j < v->row_count; ++j) {
      for (size_t k = 0; k < v->rows[j].run_count; ++k) {
        const cog_tui_md_run_t *r = &v->rows[j].runs[k];
        assert(r->bytes < capacity - used);
        memcpy(out + used, r->text, r->bytes);
        used += r->bytes;
      }
      assert(used + 1 < capacity);
      out[used++] = '\n';
    }
  }
  out[used] = '\0';
}
int main(void) {
  assert(setlocale(LC_CTYPE, "C.UTF-8"));
  cog_tui_markdown_t *md = NULL;
  assert(cog_tui_markdown_new(&md, NULL) == COG_OK);
  cog_tui_transcript_t *t = NULL;
  assert(cog_tui_transcript_new(&t, md) == COG_OK);
  cog_cli_display_item_t items[4] = {{.node_id = 1,
                                      .role = COG_MESSAGE_ROLE_USER,
                                      .status = COG_MESSAGE_COMPLETE,
                                      .text = "question",
                                      .text_len = 8},
                                     {.node_id = 0,
                                      .role = COG_MESSAGE_ROLE_ASSISTANT,
                                      .status = COG_MESSAGE_INCOMPLETE,
                                      .text = "**alpha",
                                      .text_len = 7}};
  cog_cli_display_snapshot_t snapshot = {.session_id = "test",
                                         .generation = 1,
                                         .through_sequence = 1,
                                         .items = items,
                                         .item_count = 2};
  bool changed = false;
  assert(cog_tui_transcript_sync(t, &snapshot, &changed) == COG_OK && changed);
  uint64_t active, key;
  cog_tui_md_phase_t phase;
  assert(cog_tui_transcript_identity(t, 1, &active, &phase) == COG_OK &&
         phase == COG_TUI_MD_STREAMING);
  cog_tui_screen_scene_t scene;
  assert(cog_tui_transcript_compose(t, &snapshot, 12, 40, &scene) == COG_OK);
  char text[4096];
  content(&scene, text, sizeof(text));
  assert(strstr(text, "**alpha"));
  assert(cog_tui_transcript_sync(t, &snapshot, &changed) == COG_OK && !changed);
  items[1].text = "**bravo";
  ++snapshot.through_sequence;
  assert(cog_tui_transcript_sync(t, &snapshot, &changed) == COG_OK && changed);
  assert(cog_tui_transcript_compose(t, &snapshot, 12, 40, &scene) == COG_OK);
  content(&scene, text, sizeof(text));
  assert(strstr(text, "**bravo") && !strstr(text, "alpha"));
  items[1].node_id = 2;
  items[1].status = COG_MESSAGE_COMPLETE;
  items[1].text = "done";
  items[1].text_len = 4;
  ++snapshot.through_sequence;
  assert(cog_tui_transcript_sync(t, &snapshot, &changed) == COG_OK && changed);
  assert(cog_tui_transcript_identity(t, 1, &key, &phase) == COG_OK &&
         key == active && phase == COG_TUI_MD_COMPLETE);
  assert(cog_tui_transcript_compose(t, &snapshot, 12, 40, &scene) == COG_OK);
  content(&scene, text, sizeof(text));
  assert(strstr(text, "done") && !strstr(text, "bravo"));
  snapshot.generation = 2;
  assert(cog_tui_transcript_sync(t, &snapshot, &changed) == COG_OK && !changed);
  assert(cog_tui_transcript_identity(t, 1, &key, &phase) == COG_OK &&
         key == active);
  items[2] = (cog_cli_display_item_t){.node_id = 3,
                                      .role = COG_MESSAGE_ROLE_USER,
                                      .status = COG_MESSAGE_COMPLETE,
                                      .text = "next",
                                      .text_len = 4};
  items[3] = (cog_cli_display_item_t){
      .node_id = 0,
      .role = COG_MESSAGE_ROLE_ASSISTANT,
      .status = COG_MESSAGE_INCOMPLETE,
      .text = "one\ntwo\nthree\nfour\nfive\nsix\nseven\neight\nnine\nten\n",
      .text_len = 49};
  items[3].text_len = strlen(items[3].text);
  snapshot.item_count = 4;
  assert(cog_tui_transcript_sync(t, &snapshot, &changed) == COG_OK && changed);
  assert(cog_tui_transcript_identity(t, 3, &key, &phase) == COG_OK &&
         key != active);
  assert(cog_tui_transcript_compose(t, &snapshot, 5, 30, &scene) == COG_OK);
  assert(cog_tui_transcript_scroll(t, -1) == COG_OK);
  assert(cog_tui_transcript_compose(t, &snapshot, 5, 30, &scene) == COG_OK);
  assert(strstr(scene.status, "paused"));
  uint64_t anchor = scene.slices[0].view->message_key;
  size_t row = scene.slices[0].view->first_row;
  snapshot.generation = 3;
  assert(cog_tui_transcript_sync(t, &snapshot, &changed) == COG_OK);
  assert(cog_tui_transcript_compose(t, &snapshot, 5, 35, &scene) == COG_OK);
  assert(scene.slices[0].view->message_key == anchor &&
         scene.slices[0].view->first_row == row);
  items[3].node_id = 4;
  items[3].status = COG_MESSAGE_CANCELLED;
  assert(cog_tui_transcript_sync(t, &snapshot, &changed) == COG_OK && changed);
  uint64_t committed;
  assert(cog_tui_transcript_identity(t, 3, &committed, &phase) == COG_OK &&
         committed == key && phase == COG_TUI_MD_CANCELLED);
  assert(cog_tui_transcript_scroll(t, 0) == COG_OK);
  assert(cog_tui_transcript_compose(t, &snapshot, 5, 30, &scene) == COG_OK &&
         !strstr(scene.status, "paused"));
  items[3].text = "|a|b|\n|-|-|\n|x|y|overflow|";
  items[3].text_len = strlen(items[3].text);
  assert(cog_tui_transcript_sync(t, &snapshot, &changed) == COG_OK);
  assert(cog_tui_transcript_compose(t, &snapshot, 12, 40, &scene) == COG_OK);
  content(&scene, text, sizeof(text));
  assert(strstr(text, "overflow") && strstr(scene.status, "Plain"));
  size_t large_size = 4u * 1024u * 1024u + 64u;
  char *large = malloc(large_size);
  assert(large);
  memset(large, 'x', large_size);
  memcpy(large + large_size - 12, "\nLARGE-TAIL\n", 12);
  items[3].text = large;
  items[3].text_len = large_size;
  assert(cog_tui_transcript_sync(t, &snapshot, &changed) == COG_OK);
  assert(cog_tui_transcript_compose(t, &snapshot, 12, 40, &scene) == COG_OK);
  content(&scene, text, sizeof(text));
  assert(strstr(text, "LARGE-TAIL"));
  assert(cog_tui_transcript_sync(t, &snapshot, &changed) == COG_OK && changed);
  free(large);
  items[3].text = "";
  items[3].text_len = 0;
  items[3].node_id = 0;
  items[3].text = "```c\nunfinished";
  items[3].text_len = strlen(items[3].text);
  assert(cog_tui_transcript_sync(t, &snapshot, &changed) == COG_OK);
  assert(cog_tui_transcript_fail(t) == COG_OK);
  assert(cog_tui_transcript_sync(t, &snapshot, &changed) == COG_OK);
  assert(cog_tui_transcript_identity(t, 3, &key, &phase) == COG_OK &&
         phase == COG_TUI_MD_FAILED);
  assert(cog_tui_transcript_compose(t, &snapshot, 12, 40, &scene) == COG_OK);
  assert(scene.slices[scene.slice_count - 1].view->phase == COG_TUI_MD_FAILED);
  items[3].node_id = items[0].node_id;
  assert(cog_tui_transcript_sync(t, &snapshot, &changed) == COG_ERROR_PROTOCOL);
  items[3].node_id = 0;
  snapshot.session_id = "other";
  assert(cog_tui_transcript_sync(t, &snapshot, &changed) == COG_ERROR_PROTOCOL);
  snapshot.session_id = "test";
  assert(cog_tui_transcript_scroll(t, 2) == COG_ERROR_INVALID_ARG);
  assert(cog_tui_transcript_compose(t, &snapshot, 0, 40, &scene) ==
         COG_ERROR_INVALID_ARG);
  assert(cog_tui_transcript_free(&t) == COG_OK && !t);
  assert(cog_tui_markdown_free(&md) == COG_OK);
  return 0;
}
