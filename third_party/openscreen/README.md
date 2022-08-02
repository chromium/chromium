# README.md for Open Screen Library in Chromium

openscreen is built in Chromium with some build differences based on the value
of the GN argument `build_with_chromium`.  `build_with_chromium` is defined in
`//build_overrides/build.gni` and is `true` when openscreen is built as part of
Chromium and `false` when built standalone.
