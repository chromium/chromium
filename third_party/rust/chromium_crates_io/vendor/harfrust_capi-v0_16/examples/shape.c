/* Shapes a string with a font and prints the resulting glyphs.
 *
 * Build against the static library, after `cargo build -p harfrust_capi
 * --release`:
 *
 *   cc -I harfrust_capi/include harfrust_capi/examples/shape.c \
 *      target/release/libharfrust_c.a -lm -o shape
 *
 * On Windows, with MSVC:
 *
 *   cl /I harfrust_capi\include harfrust_capi\examples\shape.c ^
 *      target\release\harfrust_c.lib ws2_32.lib userenv.lib ntdll.lib
 *
 * Then:
 *
 *   ./shape font.ttf "Hello, world!"
 */

#include <hr.h>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: %s FONT-FILE TEXT\n", argv[0]);
    return 2;
  }

  /* Constructors never return NULL; on failure they hand back an immortal
     empty object, so a whole chain can be written without checking each
     step. Use the _or_fail variants when you want NULL instead. */
  hr_blob_t *blob = hr_blob_create_from_file_or_fail(argv[1]);
  if (blob == NULL) {
    fprintf(stderr, "could not read %s\n", argv[1]);
    return 1;
  }

  hr_face_t *face = hr_face_create(blob, 0);
  hr_font_t *font = hr_font_create(face);

  /* Positions come back in font units unless a scale is set. For 26.6 fixed
     point at 16px, this would be hr_font_set_scale(font, 16 * 64, 16 * 64). */
  printf("upem %u, %u glyphs\n", hr_face_get_upem(face),
         hr_face_get_glyph_count(face));

  hr_buffer_t *buffer = hr_buffer_create();
  hr_buffer_add_utf8(buffer, argv[2], -1, 0, -1);

  /* Fill in direction, script and language from the text itself. Set them
     explicitly instead when you already know them. */
  hr_buffer_guess_segment_properties(buffer);

  hr_shape(font, buffer, NULL, 0);

  unsigned int count = 0;
  hr_glyph_info_t *infos = hr_buffer_get_glyph_infos(buffer, &count);
  hr_glyph_position_t *positions = hr_buffer_get_glyph_positions(buffer, &count);

  for (unsigned int i = 0; i < count; i++) {
    printf("  gid %-6u cluster %-4u advance %-6d offset %d,%d",
           infos[i].codepoint, infos[i].cluster, positions[i].x_advance,
           positions[i].x_offset, positions[i].y_offset);
    if (hr_glyph_info_get_glyph_flags(&infos[i]) &
        HR_GLYPH_FLAG_UNSAFE_TO_BREAK) {
      printf("  unsafe-to-break");
    }
    printf("\n");
  }

  /* And the same run in HarfBuzz's text serialization format. */
  char line[4096];
  unsigned int consumed = 0;
  if (hr_buffer_serialize_glyphs(buffer, 0, count, line, sizeof line, &consumed,
                                 font, HR_BUFFER_SERIALIZE_FORMAT_TEXT,
                                 HR_BUFFER_SERIALIZE_FLAG_DEFAULT) > 0) {
    printf("%s\n", line);
  }

  /* Shaping again through an explicit plan. hr_shape() already reuses plans
     from a per-face cache, so this matters only when you want to hold one
     yourself; hr_shape_plan_create_cached draws from that same cache. */
  hr_segment_properties_t props = HR_SEGMENT_PROPERTIES_DEFAULT;
  hr_buffer_get_segment_properties(buffer, &props);

  hr_shape_plan_t *plan =
      hr_shape_plan_create_cached(face, &props, NULL, 0, NULL);

  hr_buffer_t *again = hr_buffer_create();
  hr_buffer_add_utf8(again, argv[2], -1, 0, -1);
  hr_buffer_set_segment_properties(again, &props);

  /* The properties came from the buffer we just shaped, and the font is the
     one the plan was built over, so this applies. Executing a plan that does
     not apply aborts, the way HarfBuzz's assertions do. */
  if (hr_shape_plan_execute(plan, font, again, NULL, 0)) {
    unsigned int planned = 0;
    hr_buffer_get_glyph_infos(again, &planned);
    printf("%u glyphs via %s plan\n", planned,
           hr_shape_plan_get_shaper(plan));
  }

  hr_buffer_destroy(again);
  hr_shape_plan_destroy(plan);

  hr_buffer_destroy(buffer);
  hr_font_destroy(font);
  hr_face_destroy(face);
  hr_blob_destroy(blob);
  return 0;
}
