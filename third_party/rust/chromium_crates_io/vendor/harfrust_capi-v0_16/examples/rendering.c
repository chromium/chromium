/* The same C99 consumer can be built against HarfRust or HarfBuzz. */
#ifdef USE_HARFBUZZ
#include <hb.h>
#else
#include <hr-hb-paint.h>
#include <hr-hb-draw.h>
#include <hr-hb-subset.h>
#endif
#include <assert.h>
#include <stddef.h>

typedef char draw_state_size[sizeof(hb_draw_state_t) == 48 ? 1 : -1];
typedef char color_stop_size[sizeof(hb_color_stop_t) == 12 ? 1 : -1];
typedef char color_line_size[sizeof(hb_color_line_t) == 13 * sizeof(void *) ? 1 : -1];

static void line(hb_draw_funcs_t *funcs, void *data, hb_draw_state_t *state,
                 float x, float y, void *user_data)
{
  (void)funcs; (void)x; (void)y; (void)user_data;
  assert(state->path_open);
  ++*(unsigned *)data;
}

static void color(hb_paint_funcs_t *funcs, void *data, hb_bool_t foreground,
                  hb_color_t value, void *user_data)
{
  (void)funcs; (void)foreground; (void)user_data;
  assert(hb_color_get_alpha(value) == 255);
  ++*(unsigned *)data;
}

int main(int argc, char **argv)
{
  hb_blob_t *blob;
  hb_face_t *face;
  hb_font_t *font;
  hb_draw_funcs_t *draw;
  hb_paint_funcs_t *paint;
  hb_draw_state_t state = HB_DRAW_STATE_DEFAULT;
  unsigned lines = 0, colors = 0;
  assert(argc == 2);
  blob = hb_blob_create_from_file(argv[1]);
  face = hb_face_create(blob, 0);
  font = hb_font_create(face);
  draw = hb_draw_funcs_create();
  paint = hb_paint_funcs_create();
  hb_draw_funcs_set_line_to_func(draw, line, NULL, NULL);
  hb_paint_funcs_set_color_func(paint, color, NULL, NULL);
  hb_draw_funcs_make_immutable(draw);
  hb_paint_funcs_make_immutable(paint);
  assert(hb_font_draw_glyph_or_fail(font, 1, draw, &lines));
  assert(lines == 3);
  assert(hb_font_paint_glyph_or_fail(font, 2, paint, &colors, 0,
                                   HB_COLOR(0xaa, 0xbb, 0xcc, 0xff)));
  assert(colors == 2);
  hb_draw_move_to(draw, &lines, &state, 0, 0);
  hb_draw_line_to(draw, &lines, &state, 1, 1);
  hb_draw_close_path(draw, &lines, &state);
  assert(!state.path_open && lines == 5);
  hb_paint_funcs_destroy(paint);
  hb_draw_funcs_destroy(draw);
  hb_font_destroy(font);
  hb_face_destroy(face);
  hb_blob_destroy(blob);
  return 0;
}
