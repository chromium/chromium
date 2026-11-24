/* The same job as shape.c, written entirely in HarfBuzz's names.
 *
 * Nothing here mentions HarfRust. Including <hr-hb.h> in place of <hb.h> is
 * the only change needed to build HarfBuzz shaping code against this library.
 *
 *   cc -I harfrust_capi/include harfrust_capi/examples/hb-compat.c \
 *      target/release/libharfrust_c.a -lm -o hb-compat
 *   ./hb-compat font.ttf "Hello, world!"
 */

#include <hr-hb.h>

#include <stdio.h>

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: %s FONT-FILE TEXT\n", argv[0]);
    return 2;
  }

  hb_blob_t *blob = hb_blob_create_from_file_or_fail(argv[1]);
  if (blob == NULL) {
    fprintf(stderr, "could not read %s\n", argv[1]);
    return 1;
  }

  hb_face_t *face = hb_face_create(blob, 0);
  hb_font_t *font = hb_font_create(face);
  hb_font_set_scale(font, 16 * 64, 16 * 64); /* 26.6 fixed point at 16px */

  hb_buffer_t *buffer = hb_buffer_create();
  hb_buffer_add_utf8(buffer, argv[2], -1, 0, -1);
  hb_buffer_guess_segment_properties(buffer);

  /* Direction predicates are macros in HarfBuzz, so they map too. */
  hb_direction_t dir = hb_buffer_get_direction(buffer);
  printf("%s, %s\n", hb_direction_to_string(dir),
         HB_DIRECTION_IS_HORIZONTAL(dir) ? "horizontal" : "vertical");

  /* As does HB_TAG, used here to turn off ligatures. */
  hb_feature_t no_liga;
  no_liga.tag = HB_TAG('l', 'i', 'g', 'a');
  no_liga.value = 0;
  no_liga.start = HB_FEATURE_GLOBAL_START;
  no_liga.end = HB_FEATURE_GLOBAL_END;

  if (!hb_shape_full(font, buffer, &no_liga, 1, NULL)) {
    fprintf(stderr, "shaping ran out of room\n");
    return 1;
  }

  unsigned int count = 0;
  hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buffer, &count);
  hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer, &count);

  for (unsigned int i = 0; i < count; i++) {
    printf("  gid %-6u cluster %-4u advance %d\n", infos[i].codepoint,
           infos[i].cluster, positions[i].x_advance);
  }

  /* And a shape plan, over the properties the buffer settled on. */
  hb_segment_properties_t props = HB_SEGMENT_PROPERTIES_DEFAULT;
  hb_buffer_get_segment_properties(buffer, &props);
  hb_shape_plan_t *plan =
      hb_shape_plan_create_cached(face, &props, NULL, 0, NULL);
  printf("plan shaper: %s\n", hb_shape_plan_get_shaper(plan));

  hb_shape_plan_destroy(plan);
  hb_buffer_destroy(buffer);
  hb_font_destroy(font);
  hb_face_destroy(face);
  hb_blob_destroy(blob);
  return 0;
}
