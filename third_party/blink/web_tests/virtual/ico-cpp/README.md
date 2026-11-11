# C++ ICO layout compatibility

Runs the shared ICO intrinsic-size and automatic-layout regression with
`RustyIcoFeature` disabled. The normal test runs with Rust ICO enabled by
the field-trial testing configuration. Both runs use identical expectations
to check compatibility with the legacy decoder.

This is a temporary baseline during the Rust decoder transition. The suite
expires on May 10, 2027, and should be removed when the legacy decoder is retired.
