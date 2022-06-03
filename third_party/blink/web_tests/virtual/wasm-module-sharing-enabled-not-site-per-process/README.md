This suite runs the tests

external/wpt/wasm/serialization/module/window-similar-but-cross-origin-success.sub.html
external/wpt/wasm/serialization/module/window-domain-success.sub.html

with --disable-site-isolation-trials --cross-origin-webassembly-module-sharing-allowed.

For more details on --disable-site-isolation-trials, please take a look at
virtual/not-site-per-process/README.md.

This suite tests that cross-origin WebAssembly module sharing still works
correctly with the enterprise policy CrossOriginWebAssemblyModuleSharingEnabled.
code caches. Tracking bug: crbug.com/1231101
