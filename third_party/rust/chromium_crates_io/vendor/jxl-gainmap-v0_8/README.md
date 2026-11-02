# `jxl-gainmap`

`jxl-gainmap` provides helpers for inspecting the payload of a JPEG XL `jhgm`
gain map box. `JxlGainMap` borrows metadata and the nested image while decoding
the selected optional alternate color profile during parsing. Metadata
interpretation, nested image decoding, and gain map rendering are not the scope
of this crate.

The core `jxl` decoder exposes captured auxiliary boxes through
`JxlAuxBoxType::HDR_GAIN_MAP`; if the enclosing `brob` box is Brotli-compressed,
enable `jxl`'s `brotli` feature and decompress it with `JxlAuxBox::data` first.
