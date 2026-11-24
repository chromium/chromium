#include "hr-hb-subset.h"
#include <stdio.h>

int main(int argc, char **argv) {
  if (argc != 2) return 2;
  hb_blob_t *blob = hb_blob_create_from_file(argv[1]);
  hb_face_t *face = hb_face_create(blob, 0);
  hb_blob_destroy(blob);
  unsigned original_count = hb_face_get_glyph_count(face);
  hb_face_t *preprocessed = hb_subset_preprocess(face);
  hb_subset_input_t *input = hb_subset_input_create_or_fail();
  hb_set_add(hb_subset_input_unicode_set(input), 'A');
  hb_subset_input_set_flags(input, HB_SUBSET_FLAGS_RETAIN_GIDS | HB_SUBSET_FLAGS_NOTDEF_OUTLINE);
  hb_font_t *source_font = hb_font_create(face);
  hb_codepoint_t source_glyph = 0;
  int found = hb_font_get_nominal_glyph(source_font, 'A', &source_glyph);
  hb_font_destroy(source_font);
  hb_subset_plan_t *plan = hb_subset_plan_create_or_fail(face, input);
  hb_map_t *mapping = plan ? hb_subset_plan_old_to_new_glyph_mapping(plan) : NULL;
  unsigned planned_glyph = mapping ? hb_map_get(mapping, source_glyph) : 0;
  hb_face_t *subset = hb_subset_plan_execute_or_fail(plan);
  hb_subset_plan_destroy(plan);
  hb_face_destroy(face);
  hb_subset_input_destroy(input);
  if (!subset || !preprocessed) return 1;
  hb_font_t *font = hb_font_create(subset);
  hb_codepoint_t output_glyph = 0;
  int mapped = hb_font_get_nominal_glyph(font, 'A', &output_glyph);
  hb_blob_t *output = hb_face_reference_blob(subset);
  unsigned length = hb_blob_get_length(output);
  int success = found && mapped && source_glyph == output_glyph && planned_glyph == output_glyph && length > 0;
  printf("subset: %u bytes, retained glyph %u\n", length, output_glyph);
  hb_blob_destroy(output);
  hb_font_destroy(font);
  hb_face_destroy(subset);
  input = hb_subset_input_create_or_fail();
  hb_subset_input_keep_everything(input);
  hb_set_t *names = hb_subset_input_set(input, HB_SUBSET_SETS_NAME_ID);
  hb_set_clear(names);
  hb_set_add(names, 1);
  subset = hb_subset_or_fail(preprocessed, input);
  hb_face_destroy(preprocessed);
  hb_subset_input_destroy(input);
  success = success && subset && hb_face_get_glyph_count(subset) == original_count;
  hb_face_destroy(subset);
  return success ? 0 : 1;
}
