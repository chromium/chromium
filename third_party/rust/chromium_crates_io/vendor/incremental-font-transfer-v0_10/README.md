# Incremental Font Transfer

A client side implementation of the Incremental Font Transfer standard <https://w3c.github.io/IFT/Overview.html#glyph-keyed>
 
More specifically this provides:
- Implementation of parsing and reading incremental font patch mappings:
  <https://w3c.github.io/IFT/Overview.html#font-format-extensions>
- Implementation of parsing and apply incremental font patches:
  <https://w3c.github.io/IFT/Overview.html#font-patch-formats>

Built on top of the read-fonts crate.

## Panicking

This library should not panic regardless of API misuse or use of
corrupted/malicious font files. Please file an issue if this occurs.

## Safety

Unsafe code is forbidden by a `#![forbid(unsafe_code)]` attribute in the root
of the library.
