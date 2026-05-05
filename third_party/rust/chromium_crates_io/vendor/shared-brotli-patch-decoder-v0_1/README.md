# Shared Brotli Patch Decoder

This crate provides a wrapper around brotlic-sys to provide a method for decoding brotli encoded data which utilizes a shared raw
LZ77 dictionary. This is an extension to standard brotli decoding, which is specified here: https://datatracker.ietf.org/doc/html/draft-vandevenne-shared-brotli-format-11#section-3.2

This is meant to primarily be used in the context of decoding incremental font transfer patches (see: https://w3c.github.io/IFT/Overview.html#table-keyed).
