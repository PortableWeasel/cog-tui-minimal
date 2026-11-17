#define _XOPEN_SOURCE 700
#include <cog_tui/screen.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <wchar.h>

struct cog_tui_screen {
  FILE *input, *output;
  cog_tui_screen_size_t size;
  char *label, *draft;
  size_t bytes, cursor;
};
typedef struct glyph {
  char text[16];
  size_t bytes, consumed, cells;
} glyph_t;
/* Invalid encodings and invisible terminal controls are printable literals. */
static glyph_t glyph(const char *text, size_t count, bool combining) {
  glyph_t g = {{0}, 0, 1, 0};
  unsigned char c = (unsigned char)text[0];
  wchar_t wc = 0;
  mbstate_t state = {0};
  size_t n = mbrtowc(&wc, text, count, &state);
  if (n == (size_t)-1 || n == (size_t)-2 || n == 0 || c < 32u || c == 127u) {
    int k = snprintf(g.text, sizeof(g.text), "\\x%02x", (unsigned)c);
    g.bytes = (size_t)k;
    g.cells = g.bytes;
    return g;
  }
  g.consumed = n;
  int width = wcwidth(wc);
  if ((wc >= 0x80 && wc <= 0x9f) || width < 0 || wc == 0x200d ||
      (wc >= 0x202a && wc <= 0x202e) || (wc >= 0x2066 && wc <= 0x2069) ||
      (wc >= 0xfe00 && wc <= 0xfe0f) || (wc >= 0x1f000 && wc <= 0x1faff) ||
      (!combining && width == 0)) {
    int k = snprintf(g.text, sizeof(g.text), "\\u%04lx", (unsigned long)wc);
    g.bytes = (size_t)k;
    g.cells = g.bytes;
    return g;
  }
  if (n > sizeof(g.text)) {
    g.text[0] = '?';
    g.bytes = 1;
    g.cells = 1;
    return g;
  }
  memcpy(g.text, text, n);
  g.bytes = n;
  g.cells = (size_t)width;
  return g;
}
static size_t cells_before(const char *text, size_t count, size_t cursor) {
  size_t pos = 0, cells = 0;
  while (pos < count) {
    glyph_t g = glyph(text + pos, count - pos, false);
    if (pos + g.consumed > cursor)
      break;
    cells += g.cells;
    pos += g.consumed;
  }
  return cells;
}
/* Render only full glyphs in a virtual cell interval. */
static void text_window(FILE *out, const char *text, size_t count, size_t first,
                        size_t width) {
  size_t pos = 0, cell = 0, printed = 0;
  bool base = false;
  while (pos < count) {
    glyph_t g = glyph(text + pos, count - pos, false);
    if (cell >= first && cell + g.cells <= first + width && (g.cells || base)) {
      while (printed < cell - first) {
        fputc(' ', out);
        ++printed;
      }
      (void)fwrite(g.text, 1, g.bytes, out);
      printed += g.cells;
      if (g.cells)
        base = true;
    } else if (g.cells)
      base = false;
    cell += g.cells;
    pos += g.consumed;
    if (cell > first + width)
      break;
  }
}
static cog_result_t geometry(cog_tui_screen_t *s, cog_tui_screen_size_t *size) {
  struct winsize ws = {0};
  if (ioctl(fileno(s->input), TIOCGWINSZ, &ws) < 0)
    return COG_ERROR_IO;
  size->rows = ws.ws_row;
  size->columns = ws.ws_col;
  size->output_rows =
      size->rows >= 4 && size->columns >= 4 ? size->rows - 2 : 0;
  if (size->output_rows > 300)
    size->output_rows = 300;
  size->output_columns = size->output_rows ? size->columns - 1 : 0;
  if (size->output_columns > 1000)
    size->output_columns = 1000;
  return COG_OK;
}
cog_result_t cog_tui_screen_open(cog_tui_screen_t **out,
                                 cog_tui_screen_backend_t backend, FILE *input,
                                 FILE *output) {
  if (!out)
    return COG_ERROR_NULL_PTR;
  *out = NULL;
  if (!input || !output)
    return COG_ERROR_NULL_PTR;
  if (backend != COG_TUI_SCREEN_ANSI && backend != COG_TUI_SCREEN_NCURSESW)
    return COG_ERROR_INVALID_ARG;
  if (backend == COG_TUI_SCREEN_NCURSESW)
    return COG_ERROR_UNSUPPORTED;
  const char *term = getenv("TERM");
  if (!term || !*term || !strcmp(term, "dumb"))
    return COG_ERROR_UNSUPPORTED;
  if (!isatty(fileno(input)) || !isatty(fileno(output)))
    return COG_ERROR_INVALID_ARG;
  cog_tui_screen_t *s = calloc(1, sizeof(*s));
  if (!s)
    return COG_ERROR_NOMEM;
  s->input = input;
  s->output = output;
  cog_result_t rc = geometry(s, &s->size);
  if (rc != COG_OK) {
    free(s);
    return rc;
  }
  s->label = strdup("");
  s->draft = strdup("");
  if (!s->label || !s->draft) {
    free(s->label);
    free(s->draft);
    free(s);
    return COG_ERROR_NOMEM;
  }
  if (fputs("\033[?2004s\033[?2004h\033[0m\033[r\033[2J\033[H", output) ==
          EOF ||
      fflush(output) != 0) {
    free(s->label);
    free(s->draft);
    free(s);
    return COG_ERROR_IO;
  }
  *out = s;
  return COG_OK;
}
cog_result_t cog_tui_screen_resize(cog_tui_screen_t *screen,
                                   cog_tui_screen_size_t *size) {
  if (!screen || !size)
    return COG_ERROR_NULL_PTR;
  cog_tui_screen_size_t next;
  cog_result_t rc = geometry(screen, &next);
  if (rc != COG_OK)
    return rc;
  screen->size = next;
  *size = next;
  return COG_OK;
}
cog_result_t cog_tui_screen_input(cog_tui_screen_t *screen,
                                  const cog_tui_screen_input_t *input) {
  if (!screen || !input || !input->label)
    return COG_ERROR_NULL_PTR;
  if ((!input->bytes && input->byte_count) ||
      input->cursor_byte > input->byte_count)
    return COG_ERROR_INVALID_ARG;
  size_t label_size = strnlen(input->label, 129);
  if (label_size > 128 || input->byte_count > 1048576)
    return COG_ERROR_LIMIT;
  char *label = malloc(label_size + 1), *draft = malloc(input->byte_count + 1);
  if (!label || !draft) {
    free(label);
    free(draft);
    return COG_ERROR_NOMEM;
  }
  memcpy(label, input->label, label_size + 1);
  if (input->byte_count)
    memcpy(draft, input->bytes, input->byte_count);
  draft[input->byte_count] = '\0';
  free(screen->label);
  free(screen->draft);
  screen->label = label;
  screen->draft = draft;
  screen->bytes = input->byte_count;
  screen->cursor = input->cursor_byte;
  return COG_OK;
}
static bool safe_run(const cog_tui_md_run_t *run, size_t columns,
                     size_t previous) {
  if ((!run->text && run->bytes) || run->column < previous ||
      run->column > columns || run->cells > columns - run->column ||
      run->style > 31u || run->origin < COG_TUI_MD_SOURCE ||
      run->origin > COG_TUI_MD_PREVIEW)
    return false;
  size_t pos = 0, cells = 0;
  while (pos < run->bytes) {
    glyph_t g = glyph(run->text + pos, run->bytes - pos, true);
    if (g.bytes != g.consumed || memcmp(g.text, run->text + pos, g.bytes))
      return false;
    if (g.cells > columns || cells > columns - g.cells)
      return false;
    cells += g.cells;
    pos += g.consumed;
  }
  return cells == run->cells;
}
static void style(FILE *out, unsigned bits) {
  fputs("\033[0m", out);
  if (bits & COG_TUI_MD_BOLD)
    fputs("\033[1m", out);
  if (bits & COG_TUI_MD_ITALIC)
    fputs("\033[3m", out);
  if (bits & COG_TUI_MD_CODE)
    fputs("\033[7m", out);
  if (bits & COG_TUI_MD_DIM)
    fputs("\033[2m", out);
  if (bits & COG_TUI_MD_UNDERLINE)
    fputs("\033[4m", out);
}
cog_result_t cog_tui_screen_present(cog_tui_screen_t *s,
                                    const cog_tui_screen_scene_t *scene) {
  if (!s || !scene)
    return COG_ERROR_NULL_PTR;
  if ((!scene->slices && scene->slice_count) || scene->slice_count > 300)
    return COG_ERROR_INVALID_ARG;
  if (scene->status && strnlen(scene->status, 513) > 512)
    return COG_ERROR_LIMIT;
  cog_tui_screen_size_t actual;
  cog_result_t rc = geometry(s, &actual);
  if (rc != COG_OK)
    return rc;
  if (actual.rows != s->size.rows || actual.columns != s->size.columns)
    return COG_ERROR_AGAIN;
  size_t total = 0;
  for (size_t i = 0; i < scene->slice_count; ++i) {
    const cog_tui_screen_slice_t *slice = &scene->slices[i];
    if (!slice->view || (slice->label && strnlen(slice->label, 129) > 128))
      return COG_ERROR_INVALID_ARG;
    const cog_tui_md_view_t *view = slice->view;
    if (view->columns != actual.output_columns)
      return COG_ERROR_AGAIN;
    if ((!view->rows && view->row_count) ||
        view->row_count > actual.output_rows ||
        total > actual.output_rows - view->row_count)
      return COG_ERROR_LIMIT;
    total += view->row_count;
    if (slice->label && ++total > actual.output_rows)
      return COG_ERROR_LIMIT;
    for (size_t j = 0; j < view->row_count; ++j) {
      const cog_tui_md_row_t *row = &view->rows[j];
      if ((!row->runs && row->run_count) || row->run_count > 10000)
        return COG_ERROR_INVALID_ARG;
      size_t previous = 0;
      for (size_t k = 0; k < row->run_count; ++k) {
        if (!safe_run(&row->runs[k], actual.output_columns, previous))
          return COG_ERROR_INVALID_ARG;
        previous = row->runs[k].column + row->runs[k].cells;
      }
    }
  }
  FILE *out = s->output;
  fputs("\033[?25l\033[0m\033[H\033[2J", out);
  if (!actual.output_rows) {
    if (actual.rows && actual.columns > 1)
      text_window(out, "Resize terminal", 15, 0, actual.columns - 1);
    fputs("\033[?25h", out);
    return fflush(out) == 0 && !ferror(out) ? COG_OK : COG_ERROR_IO;
  }
  size_t y = 1;
  for (size_t i = 0; i < scene->slice_count; ++i) {
    const cog_tui_screen_slice_t *slice = &scene->slices[i];
    if (slice->label) {
      fprintf(out, "\033[%zu;1H\033[0m", y++);
      text_window(out, slice->label, strlen(slice->label), 0,
                  actual.output_columns);
    }
    for (size_t j = 0; j < slice->view->row_count; ++j, ++y) {
      const cog_tui_md_row_t *row = &slice->view->rows[j];
      for (size_t k = 0; k < row->run_count; ++k) {
        const cog_tui_md_run_t *run = &row->runs[k];
        fprintf(out, "\033[%zu;%zuH", y, run->column + 1);
        style(out, run->style);
        if (run->bytes)
          (void)fwrite(run->text, 1, run->bytes, out);
      }
    }
  }
  fprintf(out, "\033[0m\033[%zu;1H", actual.rows - 1);
  const char *status = scene->status ? scene->status : "--- input ---";
  text_window(out, status, strlen(status), 0, actual.output_columns);
  fprintf(out, "\033[%zu;1H", actual.rows);
  size_t label_cells =
      cells_before(s->label, strlen(s->label), strlen(s->label));
  size_t label_width = actual.output_columns / 2;
  if (label_cells < label_width)
    label_width = label_cells;
  text_window(out, s->label, strlen(s->label), 0, label_width);
  size_t draft_width = actual.output_columns - label_width;
  size_t cursor = cells_before(s->draft, s->bytes, s->cursor);
  size_t first = cursor >= draft_width ? cursor - draft_width + 1 : 0;
  fprintf(out, "\033[%zu;%zuH", actual.rows, label_width + 1);
  text_window(out, s->draft, s->bytes, first, draft_width);
  fprintf(out, "\033[0m\033[%zu;%zuH\033[?25h", actual.rows,
          label_width + cursor - first + 1);
  return fflush(out) == 0 && !ferror(out) ? COG_OK : COG_ERROR_IO;
}
cog_result_t cog_tui_screen_close(cog_tui_screen_t **screen) {
  if (!screen)
    return COG_ERROR_NULL_PTR;
  if (!*screen)
    return COG_OK;
  cog_tui_screen_t *s = *screen;
  *screen = NULL;
  fprintf(s->output,
          "\033[?2004l\033[?2004r\033[0m\033[r\033[?25h\033[%zu;1H\033[2K\r\n",
          s->size.rows ? s->size.rows : 1);
  bool failed = fflush(s->output) != 0 || ferror(s->output);
  free(s->label);
  free(s->draft);
  free(s);
  return failed ? COG_ERROR_IO : COG_OK;
}
