# harfrust_capi

A C API for [HarfRust](https://github.com/harfbuzz/harfrust), mirroring the
shaping, OpenType BASE baseline, and MATH query APIs of HarfBuzz with an `hr_`
prefix in place of `hb_`.

Every type, function, enumerator and constant is named and numbered to match
its HarfBuzz counterpart, so C code can usually be ported by renaming `hb_` to
`hr_`. The prefix also means this library can be linked into the same process
as HarfBuzz itself without collisions.

```c
#include <hr.h>

hr_blob_t   *blob   = hr_blob_create_from_file("font.ttf");
hr_face_t   *face   = hr_face_create(blob, 0);
hr_font_t   *font   = hr_font_create(face);
hr_buffer_t *buffer = hr_buffer_create();

hr_buffer_add_utf8(buffer, "Hello", -1, 0, -1);
hr_buffer_guess_segment_properties(buffer);
hr_shape(font, buffer, NULL, 0);

unsigned int count;
hr_glyph_info_t     *infos     = hr_buffer_get_glyph_infos(buffer, &count);
hr_glyph_position_t *positions = hr_buffer_get_glyph_positions(buffer, &count);
```

See [`examples/shape.c`](examples/shape.c) for a complete program.

## Drop-in replacement for HarfBuzz

[`include/hr-hb.h`](include/hr-hb.h) maps every HarfBuzz name onto its
HarfRust counterpart. Include it in place of `<hb.h>` and existing HarfBuzz
shaping code builds unchanged:

```c
#include <hr-hb.h>

hb_blob_t   *blob   = hb_blob_create_from_file("font.ttf");
hb_face_t   *face   = hb_face_create(blob, 0);
hb_font_t   *font   = hb_font_create(face);
hb_buffer_t *buffer = hb_buffer_create();

hb_buffer_add_utf8(buffer, "Hello", -1, 0, -1);
hb_buffer_guess_segment_properties(buffer);
hb_shape(font, buffer, NULL, 0);
```

[`examples/hb-compat.c`](examples/hb-compat.c) is the same program as
`shape.c` written entirely in HarfBuzz's names, and mentions HarfRust nowhere.

Two things to know. It cannot be combined with HarfBuzz itself in one
translation unit, since the macros would rewrite HarfBuzz's own declarations;
include one or the other. It covers the APIs listed below, so unsupported
HarfBuzz calls fail to compile.

The header is generated from `hr.h`, so the two cannot drift:

```sh
python3 scripts/gen-hb-compat-header.py
```

## Building

```sh
cargo build -p harfrust_capi --release
```

This produces static and shared C libraries. A Rust `rlib` is deliberately not
emitted alongside them, because that makes Cargo suppress release LTO for the
C libraries. The header is committed at [`include/hr.h`](include/hr.h) and is
generated with [cbindgen](https://github.com/mozilla/cbindgen):

```sh
cbindgen --config harfrust_capi/cbindgen.toml \
         --crate harfrust_capi \
         --output harfrust_capi/include/hr.h
```

Regenerate it after changing any `pub extern "C"` item.

## Optional drawing and painting

Enable `draw` for Skrifa-backed outline extraction, or `paint` for color glyph
painting (`paint` also enables `draw`):

```sh
cargo build -p harfrust_capi --release --features draw,paint
```

These features are off by default. They use the same `hr_font_t`, `hr_face_t`
and C library as shaping. Include `hr-draw.h` or `hr-paint.h` for native names,
or `hr-hb-draw.h` / `hr-hb-paint.h` for HarfBuzz source compatibility. The paint
header includes drawing. Subsetting can be enabled alongside either feature.

`hr_font_draw_glyph_or_fail` extracts unhinted glyf, CFF, CFF2 and VARC outlines
at the font's current normalized variation coordinates and independent X/Y
scales. Draw callbacks receive HarfBuzz-style contour state, with quadratic
to cubic fallback. Both the legacy custom font callbacks and callbacks that
return success are supported, including inheritance from a parent font with
different scales.

`hr_font_paint_glyph_or_fail` traverses COLRv0/v1 paint graphs, including
palettes, gradients, transforms, clips, composites and cached color glyph
callbacks. SVG documents and CBDT/sbix PNG or BGRA images are delivered to the
client's image callback. The client renders those formats and can retain an
image with `hr_blob_reference`; images share the face's rendering cache rather
than copying it, so a retained image keeps that cache alive until it is
released. Gradient color lines are only valid during their callback. Missing
`fill_glyph` and `push_group_for` callbacks use the HarfBuzz defaults.
Malformed paint graphs return false and unwind open transforms, clips and
groups.

Rendering reads known tables directly and caches an assembled SFNT once per
face. Callback faces work without table enumeration. This cache copies the
rendering tables, so the feature has a per-face memory cost beyond shaping.

Skrifa's outlines are geometrically compatible with HarfBuzz; redundant
closing lines and CFF composite contour order can differ. VARC transform
precision and bitmap extent rounding can also differ slightly. Hinting,
bitmap masks, draw shape helpers, configurable draw/paint budgets, gradient
preprocessing/tiling helpers and
the deprecated `hb_font_get_glyph_shape` names are not exposed. Skrifa's own
paint traversal limits remain in effect.

Regenerate these headers and their aliases after changing the feature APIs:

```sh
cbindgen --config harfrust_capi/cbindgen-draw.toml \
         harfrust_capi/src/draw.rs --output harfrust_capi/include/hr-draw.h
cbindgen --config harfrust_capi/cbindgen-paint.toml \
         harfrust_capi/src/paint.rs --output harfrust_capi/include/hr-paint.h
python3 scripts/gen-hb-compat-header.py
```

[`examples/rendering.c`](examples/rendering.c) exercises both features as a
C99 consumer. `tests/compare_draw_hb.py` and `tests/compare_paint_hb.py` take
HarfRust and HarfBuzz shared library paths for differential checks.

Packed colors use `hr_color_t` and `HR_COLOR(b, g, r, a)`, with channel
accessors `hr_color_get_blue/green/red/alpha`. These common definitions are
available without enabling painting.

## Object lifetime

Objects are reference counted. Constructors return a new reference the caller
owns; `hr_*_reference` takes another, and `hr_*_destroy` gives one back.

`hr_*_create` never returns `NULL`: on failure it returns an immortal empty
object, and the `_or_fail` variants return `NULL` instead. Getters accept
`NULL` too, behaving as though passed the empty object. As in HarfBuzz this
means a chain of calls can be written without checking every result, and the
error surfaces as an empty shaping result rather than a crash.

Objects can carry `user_data` keyed by the address of an `hr_user_data_key_t`,
and can be frozen with `hr_*_make_immutable`, after which setters are ignored.

## What is covered

Blobs, faces, fonts, font callbacks, buffers, shape plans and `hr_shape`,
along with the tags, directions, scripts, languages, features and variations
they need. OpenType `BASE` baseline queries are available through
`hr_ot_layout_get_baseline` and its related functions. OpenType `MATH` queries
are available through `hr_ot_math_*` functions. Layout lookup queries support
glyph participation checks. Script selection and language-specific feature lookup
are available with `hr_ot_layout_table_select_script` and
`hr_ot_layout_language_find_feature`; `hr_ot_layout_table_get_feature_tags`
enumerates all feature records, including duplicate tags. CPAL palette queries
are exposed through `hr_ot_color_has_palettes` and
`hr_ot_color_palette_get_count/flags/colors`.
Variation-axis metadata is exposed by `hr_ot_var_get_axis_count/infos`;
`hr_ot_name_get_utf16` retrieves localized names with HarfBuzz's encoding
preferences, platform language IDs, and UTF-16 buffer sizing. Unicode-platform
names can use Apple's `ltag` table; Mac Roman names follow HarfBuzz's ASCII
interpretation. AAT feat-table metadata is exposed by
`hr_aat_layout_get_feature_types` and
`hr_aat_layout_feature_type_get_selector_infos`, including exclusive defaults
and non-exclusive enable/disable pairs. The complete HarfBuzz set and map APIs
are available as `hr_set_*` and `hr_map_*`, including inverted sets, ranges, set algebra, and
iteration. OpenType script and language tags can be converted with
`hr_ot_tag_to_language` and `hr_ot_tags_from_script_and_language`.
Set `hr_font_set_ppem` when `BASE` or `MATH` device adjustments should apply; it is
independent of the point size set by `hr_font_set_ptem`.

The default Unicode provider exposes script lookup through
`hr_unicode_funcs_get_default` and `hr_unicode_script`, using the same data
as shaping. Custom Unicode callbacks remain unsupported.

Faces can be built two ways: over a blob with `hr_face_create`, or from a
callback with `hr_face_create_for_tables`, which asks for one table at a time.
Use `hr_face_set_get_table_tags_func` to enumerate the tables of a callback
face; blob faces enumerate their SFNT directory automatically.
`hr_face_set_index` changes the reported index without selecting new tables.
`hr_face_collect_unicodes` adds the selected cmap subtable's coverage to a set.

`hr_shape` already reuses shape plans through a per-face cache, the way
`hb_shape` does internally, so reach for `hr_shape_plan_create` only when you
want to hold a plan yourself. `hr_shape_plan_create_cached` draws from that
same cache.

## What is not

The following HarfBuzz APIs are not exposed:

- Layout queries beyond the available GSUB/GPOS presence, lookup count, and
  glyph collection functions.
- Custom Unicode callbacks (`hb_unicode_funcs_t`); HarfRust's own Unicode data
  is always used.
- `hb_buffer_diff`, buffer message callbacks, and `hb_font_get_glyph_name`.
- The buffer's replacement codepoint (`hb_buffer_set_replacement_codepoint`);
  invalid UTF is always replaced with U+FFFD, which is HarfBuzz's default.

## Deliberate differences from HarfBuzz

- **Set and map hashes are implementation dependent.** Equal containers have
  equal hashes, but the numeric values need not match HarfBuzz. Map iteration
  order is unspecified, as in HarfBuzz.

- **Allocation failure aborts** through Rust's allocator. Set and map
  `allocation_successful` queries return true for ordinary objects and false
  for the inert empty singletons; they do not provide allocation recovery.

- **Table callbacks must be thread safe.** `hr_face_create_for_tables` may
  invoke its callback from whichever thread first touches a given table,
  because that is when the underlying library loads it. HarfBuzz makes no such
  demand. The same applies to font callbacks and to every `destroy` function.

- **Blobs are read-only.** `hr_blob_get_data_writable` always returns `NULL`;
  shaping never needs to modify font data in place. Use
  `hr_blob_copy_writable_or_fail` for a private, mutable copy.

- **Flag types, `hr_script_t` and `hr_direction_t` are integer typedefs**
  rather than enums. C combines flags with a bitwise or, and fills a
  `hr_segment_properties_t` in itself, so neither can be held in a Rust
  enumeration without inviting undefined behaviour on values it does not list.
  The constants carry the same values as HarfBuzz's enumerators and still work
  as `switch` case labels.

- **Misusing the shaping calls aborts,** as HarfBuzz's assertions do, rather
  than being reported. `hr_shape` and `hr_shape_full` abort when handed a
  buffer that already holds glyphs or a font with nothing to shape with, and
  `hr_shape_plan_execute` aborts on a plan built for another face, or for
  properties the buffer does not carry. HarfBuzz compiles its assertions out
  with `NDEBUG`; these are always on, because `hr_shape` returns nothing and
  could not otherwise report them at all. Variation settings are not among
  them: HarfBuzz does not compare those, and shapes with the font's whatever
  the plan was built for.

  Running past the length, operation or nesting limits is not in that set
  either, and does not fail the call. HarfBuzz's shapers report success
  whatever became of the buffer, so `hr_shape_full` returns true and
  `hr_shape` carries on; `hr_buffer_allocation_successful` is what reports
  the condition. `hr_shape_full` returns false only when no shaper could be
  run at all, or when the shaper panicked -- a bug in this library -- in
  which case the buffer comes back cleared rather than half shaped.

- **Only the text serialization format** is supported by
  `hr_buffer_serialize_glyphs`; asking for JSON serializes nothing.

- **The plan cache is bounded** at 32 entries per face, where HarfBuzz's list
  is unbounded.

- **`hr_shape` fills in unset segment properties** rather than failing, since
  building a plan requires a direction.

## Threading

Blobs, faces and fonts may be shaped with from several threads at once.
Reference counts are atomic, the per-face plan cache and the user data on every
object are behind read-write locks, and the caches HarfRust fills in while
shaping -- lazily loaded tables, glyph lookups, layout lookups -- are atomics
and one-shot cells. There are tests that shape through one shared face, and
through one shared font, from several threads; they run under Miri's data race
detector across a spread of thread interleavings.

What is not shared is anything being written:

- **A buffer belongs to one thread.** Shaping writes to it throughout, so two
  threads must not touch the same one. This matches HarfBuzz.
- **Sets and maps must not be modified while another thread accesses them.**
  Shared reads and reference counting are safe; mutations require exclusive
  access.
- **A font must not be modified while another thread shapes with it.**
  `hr_font_set_scale`, `hr_font_set_variations` and `hr_font_set_funcs` all
  write. Call them before sharing the font, then `hr_font_make_immutable` to
  have later attempts ignored rather than raced. A sub-font reads its parent's
  callbacks and scale on every query, so shaping with it is shaping with its
  parent too; `hr_font_make_immutable` freezes the whole chain for that reason.
- **The same goes for a set of callbacks.** Populate an `hr_font_funcs_t`
  before installing it, and `hr_font_funcs_make_immutable` to hold it that way.

Callbacks are handed the font they were installed on so they can read its
scale and variation settings. They must not modify it, nor free it, nor touch
the buffer being shaped.

## License

MIT, the same as the rest of the project.

Nominal glyph callbacks support both scalar and strided batch mapping. A batch
callback supplies scalar queries when no scalar callback is installed; a local
scalar callback also takes precedence over an inherited batch callback.
Horizontal advance callbacks follow the same rule.

HarfBuzz compatibility headers are `hr-hb.h` for core APIs and `hr-hb-ot.h`
for OpenType APIs, and `hr-hb-aat.h` for AAT feature metadata. The core header also includes the OpenType mappings for
existing callers. The compatibility headers may be included in either order.

`HB_VERSION_*` and `hb_version*` report the HarfBuzz shaping release matched
by HarfRust, currently 15.1.0. `HR_VERSION_*` and `hr_version*` continue to
report the HarfRust package version. This lets source compatibility users
select HarfBuzz API paths with version checks. It does not imply that every
API from that HarfBuzz release is available; see the exclusions above.

Enable the optional `subset` feature for Skera-backed subsetting in the same
C library, using the existing face, blob, and set handles. Include `hr-subset.h`
for the native API or `hr-hb-subset.h` for HarfBuzz spellings. See
[SUBSETTING.md](SUBSETTING.md) for building, supported flags, and validation.

## HarfBuzz C++ helpers

Include `hr-hb-cplusplus.hh` in place of HarfBuzz's `hb-cplusplus.hh` for
the C++11 `hb::shared_ptr<T>` and `hb::unique_ptr<T>` ownership helpers,
their traits, and hash specializations. They adopt an existing reference;
copying a shared pointer acquires another reference. Supported core types
are blobs, buffers, faces, fonts, font functions, maps, sets, and shape plans.

Including `hr-hb-subset.h` before or after the C++ header also enables
subset inputs and plans. Build `harfrust_capi` with `--features subset`
when using those objects. Objects without a complete HarfRust lifetime API have no vtable.
