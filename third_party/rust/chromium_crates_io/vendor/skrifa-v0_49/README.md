# skrifa

[![Crates.io](https://img.shields.io/crates/v/skrifa.svg)](https://crates.io/crates/skrifa)
[![Docs](https://docs.rs/skrifa/badge.svg)](https://docs.rs/skrifa)
[![MIT/Apache 2.0](https://img.shields.io/badge/license-MIT%2FApache-blue.svg)](#license)

Skrifa ships as the font rendering backend for Chrome on Android and Linux,
and on other platforms for some font formats. See [Memory safety for web
fonts](https://developer.chrome.com/blog/memory-safety-fonts).

Skrifa draws glyphs from fonts at a size, optionally hinted, and reads
additional metadata: character mapping, metrics, names and variations. It is
built on the
[read-fonts](https://github.com/googlefonts/fontations/tree/main/read-fonts)
parser, is part of the [oxidize](https://github.com/googlefonts/oxidize)
project, and is tested glyph by glyph against FreeType.

```rust
use skrifa::{
    font::{glyph::outline::SvgSink, Font},
    DrawMemory, HintOptions, RasterTarget, ScalerFont,
};

fn draw_a(data: Vec<u8>) -> Result<(), skrifa::ScaleError> {
    // Load the first font in the file and create a bold instance.
    let font = Font::new(data, 0).unwrap();
    let bold = font.instance_builder().variations([("wght", 700.0)]).build();

    // Find the glyph for 'A'.
    let glyph_id = bold.charmap().map_unicode('A').unwrap_or_default();

    // Hint with the font's own instructions for a horizontal subpixel
    // target, at 16 pixels per em.
    let options = HintOptions::interpreter().with_target(RasterTarget::Lcd);
    let scaler = ScalerFont::hinted(bold, 16.0, options);

    if let Some(outline) = scaler.outline(glyph_id) {
        let mut path = SvgSink::new();
        let metrics = outline.draw(DrawMemory::Scratch, &mut path)?;
        println!("{path} advances by {:?}", metrics.h_advance);
    }
    Ok(())
}
```

## Fonts

A `Font` is a font at one location in its variation space. It loads from
owned or shared bytes, or from a function that supplies tables by tag, as a
platform font API does. It reads:

- OpenType and TrueType fonts and collections
- Standalone CFF font sets, including CID-keyed fonts
- Type 1 fonts, PFA or PFB, including multiple master fonts

The `font` module covers the rest of a font: the character map with Unicode
variation sequences, localized names, variation axes and named instances,
line, style and glyph metrics, glyph names, color palettes, color glyphs and
bitmap strikes.

## Glyphs

| Table    | Read  | Variations | Hinting                        |
|----------|-------|------------|--------------------------------|
| `glyf`   | ✔️    | ✔️          | TrueType interpreter, automatic |
| `CFF`    | ✔️    | -          | PostScript hints, automatic     |
| `CFF2`   | ✔️    | ✔️          | PostScript hints, automatic     |
| Type 1   | ✔️    | ✔️          | automatic                       |
| `VARC`   | ✔️    | ✔️          | automatic                       |
| `COLR`   | ✔️    | ✔️ (v1)     | -                               |
| `EBDT`   | ✔️    | -          | -                               |
| `CBDT`   | ✔️    | -          | -                               |
| `sbix`   | ✔️    | -          | -                               |

Outlines draw through `ScalerFont` into any `PathSink`. `COLR` glyphs paint
through a `PaintSink`. Bitmap glyphs come as coverage masks, BGRA pixels or
PNG data for the caller to decode.

## Hinting

`HintOptions` chooses between three engines:

- **The TrueType interpreter** runs a font's instructions, with the classic
  ClearType rules or the newer ones. The newer rules also follow the font's
  `gasp` table and let glyphs that ask for it keep fractional horizontal
  positions.
- **The PostScript hinter** runs the hints in CFF and CFF2 charstrings.
- **The automatic hinter** derives hints from the outline, for any outline
  format.

Each hints for a target: monochrome, grayscale, light grayscale, or
horizontal or vertical subpixel. A TrueType font that needs its
instructions to draw correct contours always runs them.

## Features

- `std` (default): use the standard library. Without it, enable `libm`
  for `no_std`.
- `agl`: map characters to the glyphs of Type 1 and standalone CFF fonts
  through their glyph names, with the Adobe Glyph List. Without it those
  fonts map no characters.

## Panicking

Skrifa should not panic, whatever the font data or the way it is called.
Please file an issue if it does.

## Safety

Unsafe code is forbidden by a `#![forbid(unsafe_code)]` attribute in the root
of the library.

## The name?

Following along with our theme, *skrifa* is Old Norse for "write" or "it is
written." And so it is named.

## License

Licensed under either of the [Apache License, Version
2.0](https://www.apache.org/licenses/LICENSE-2.0) or the [MIT
license](https://opensource.org/licenses/MIT), at your option.
