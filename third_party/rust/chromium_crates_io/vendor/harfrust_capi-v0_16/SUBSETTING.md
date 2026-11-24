# HarfBuzz subset C API for HarfRust

The optional `subset` feature of `harfrust_capi` adds font subsetting backed
by fontations' `skera` to the same C library as shaping. It uses the existing
`hr_face_t`, `hr_blob_t`, and `hr_set_t` handles.
It exports `hr_subset_*`, with source aliases in `hr-hb-subset.h`.
HarfBuzz's opaque handles cannot be passed to these functions.

The initial API covers Skia's PDF workflow: create an input, select glyphs,
set `RETAIN_GIDS | NOTDEF_OUTLINE`, subset a face, and reference its output
blob. Unicode selection, reference-counted inputs, and `NO_HINTING` are also
supported. Configurable input sets cover names, language IDs, layout features
and scripts, and dropped tables. `hr_subset_input_keep_everything` configures
all selections, with subsequent customization allowed. Glyph and Unicode sets start empty and are borrowed from their input;
`hr_set_reference` keeps a set alive independently.

Blob, TTC, and table-callback faces are accepted. Callback faces must install
`hr_face_set_get_table_tags_func`. Input tables are copied into a standalone
SFNT before calling Skera, so the source and input can be destroyed immediately
after a successful call. Zero-length tables are accepted.

## Building and linking

```sh
cargo build -p harfrust_capi --features subset
cc -std=c99 -Iharfrust_capi/include \
  harfrust_capi/examples/subset.c -Ltarget/debug \
  -lharfrust_c -Wl,-rpath,"$PWD/target/debug" -o /tmp/hr-subset
/tmp/hr-subset harfrust/tests/fonts/rb_custom/PT_Sans-Caption-Web-Regular.ttf
```

Link only `harfrust_c` for both shaping and subsetting. The `subset` feature
is disabled by default; default builds have no Skera or write-fonts dependency
and export no subset symbols. The subset headers remain available to consumers,
but using their functions requires a library built with `--features subset`.
The same feature applies to static and shared libraries on all platforms.

For static linking, use `libharfrust_c.a` and the platform's Rust system
libraries (on Linux: `-ldl -lpthread -lm`). Windows consumers link the matching
static library or DLL import library; no separate subset DLL is needed.

Regenerate headers with:

```sh
cbindgen --config harfrust_capi/cbindgen.toml \
  --crate harfrust_capi --output harfrust_capi/include/hr.h
cbindgen --config harfrust_capi/cbindgen-subset.toml \
  harfrust_capi/src/subset/mod.rs --output harfrust_capi/include/hr-subset.h
python3 scripts/gen-hb-compat-header.py
```

`hr_subset_preprocess` returns an immutable, self-contained copy of the face,
so its callbacks and original data can be released. It currently adds no
subsetting acceleration cache.
The subset header is generated separately from its module so `hr.h` stays
independent of optional APIs.

## Current scope

The dependencies are the published Skera 0.10 and write-fonts 0.56 release
series, which include the public Plan APIs. CFF, CFF2, and VARC
fonts return `NULL`: this release does not rewrite those outline tables.
Unsupported flag bits, missing enumeration, malformed inputs, and Skera
errors also return `NULL`, allowing Skia to embed the full font instead.
This is an initial API, not the complete HarfBuzz subset API. Axis pinning,
user-specified glyph renumbering, name overrides, and unimplemented flags are
not exposed yet. As with the shaping C API, allocation failure aborts.

## Validation

The C example verifies shared handles and a retained glyph after serialization.
The differential test requires `fontTools` and a HarfBuzz build with the subset
library and table-enumeration callback API:

```sh
python3 harfrust_capi/tests/subset/compare_skia_hb.py \
  target/debug/libharfrust_c.so \
  /path/to/libharfbuzz.so /path/to/libharfbuzz-subset.so
```

It compares blob, callback, and Unicode selections across five flag combinations
and three fonts, including a variable font, and checks mappings, advances,
outlines, retained holes, hinting, `.notdef`, input/output lifetimes, and failures.

Additional implemented flags are `NAME_LEGACY`, `SET_OVERLAPS_FLAG`,
`PASSTHROUGH_UNRECOGNIZED`, `GLYPH_NAMES`, `NO_PRUNE_UNICODE_RANGES`,
`NO_LAYOUT_CLOSURE`, and `NO_BIDI_CLOSURE`. Desubroutinization and IUP delta
optimization are not implemented by the dependency and remain unsupported.

`tests/subset/compare_controls_hb.py` takes the same library arguments and checks
15 additional cases, input-set defaults, keep-everything, inverted sets,
custom table passthrough, overlap flags, and preprocessing ownership.

Subset plans snapshot input sets and font bytes. They can be executed repeatedly
after releasing the original face and input, and expose old-to-new, new-to-old,
and Unicode-to-original glyph maps. Returned maps are borrowed and must not
be mutated; `hr_map_reference` retains them independently of a plan.
Configurable no-subset table tags now pass through unchanged; explicit table
drops and hint removal take precedence.

`tests/subset/compare_plans_hb.py` accepts the same library arguments. It compares
16 combinations of plan mappings and executes plans after changing/destroying
their inputs, retaining borrowed maps past plan destruction, and configuring
table passthrough.

Inputs and plans also support address-keyed user data. Metadata belongs to
the object only after a successful setter call; replacement and destruction
release it once, with callbacks invoked outside the lock for reentrancy.
The plan test compares these semantics with HarfBuzz and executes plans
concurrently.

### Actual Skia runtime checks

`tests/subset/compare_skia_runtime.py` compiles Skia's actual HarfBuzz shaper and PDF
subsetter twice, using either HarfBuzz or the HarfRust compatibility headers.
It requires a matching Linux GN static Skia build with PDF, FreeType, ICU,
and HarfBuzz enabled, plus fontTools and Poppler's `pdffonts`, `pdftotext`, and
`pdftoppm`. Apply [Skia change 1387436](https://skia-review.googlesource.com/c/skia/+/1387436)
to remove the UPEM setter first. Build the GN targets `skia`,
`modules/skshaper:skshaper`, and `modules/skunicode:skunicode_icu`.

```sh
python3 harfrust_capi/tests/subset/compare_skia_runtime.py \
  --skia /path/to/patched/skia --skia-build /path/to/skia/out/Runtime \
  --hb-source /path/to/harfbuzz --hb-build /path/to/harfbuzz/build \
  --hr "$PWD/target/debug/libharfrust_c.so" \
  --output /path/to/runtime-results
```

Eight cases cover three fonts, stream and table-callback access, and a
nondefault variable instance. Each checks four font size/horizontal scale
combinations, glyph IDs, clusters, positions, both glyph-0 subset policies,
hint programs, retained GIDs, and actual PDF embedding, text extraction,
and raster output. Skia's variable-font PDFs use its Type3 fallback.

The callback proxy disables both stream methods for shaping and direct
subsetting. During PDF creation it supplies the stream required by Skia's
font-descriptor code; the PDF subsetter still uses table callbacks because
`openExistingStream` remains unavailable.

The nondefault Linefont instance exposes fractional GPOS positioning drift
of about 0.00024 pixels and PDF raster differences. Glyph/cluster checks stay
exact; variable positions allow an accumulated 16.16 position unit per glyph.
Pixel comparisons fail by default. `--allow-variable-pixel-differences`
acknowledges and reports those two known raster exceptions while checking
the other cases. Optional `--hb-upem-shaper /path/to/original/SkShaper_harfbuzz.cpp`
also checks HarfBuzz's original setter against the removal, with strict
position and pixel comparisons for all eight cases.
