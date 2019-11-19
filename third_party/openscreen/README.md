# README.md for Open Screen Library in Chromium

openscreen is built in Chromium with some build differences.  The files that are
built in openscreen are determined by the following build variables.

 - `build_with_chromium`: `true` when building as part of a Chromium checkout,
 `false` otherwise.  Defined `//build_overrides/build.gni` in Chromium.
 - `use_mdns_responder`: `true` by default, `false` when `build_with_chromium`
 is `true`.  Controls whether the default mDNSResponder mDNS implementation is
 used.  Set by `openscreen/src/build/config/services.gni.`
 - `use_chromium_quic`: `true` by default, `false` when `build_with_chromium` is
 `true`.  Controls whether the Chromium-derived QUIC implementation in
 openscreen is used.  Set by `openscreen/src/build/config/services.gni`.
