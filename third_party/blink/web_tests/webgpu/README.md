# WebGPU CTS Testing

The WebGPU conformance test suite (CTS) is developed at
<https://github.com/gpuweb/cts>. It is written in TypeScript and compiled to
JavaScript to run as part of WPT.

(Note: there is no copy of the WebGPU CTS in WPT. This is because browsers are
at different stages of implementation, and it is more useful to pin a particular
revision of the CTS rather than use the latest version.)

Most of the WebGPU CTS runs on Chrome's infrastructure using the GPU Telemetry
harness (see content/test/gpu/gpu_tests/webgpu_cts_integration_test.py). Only
the reftests run using the web tests infrastructure.

An autoroller (https://autoroll.skia.org/r/webgpu-cts-chromium-autoroll) rolls the WebGPU
CTS into Chromium regularly. Part of the roll requires regenerating a few files which the
autoroller attempts to do.
1. `third_party/webgpu-cts/ts_sources.txt` is a generated file which tells GN the list of Typescript sources to be transpiled to Javascript.
1. `third_party/webgpu-cts/resource_files.txt` is a generated file which tells GN the list of resources that should be included in the test isolate for CTS test pages to load.
1. `third_party/blink/web_tests/wpt_internal/webgpu/web_platform/reftests/**/*.html` are the
reftests and reference files which run on the web tests test infrastructure.

There are dangling tests `third_party/blink/web_tests/wpt_internal/webgpu/web_platform/*.html` which are chrome specific and purposely made to avoid chrome behavior regressions besides the WebGPU CTS requirements.

### Running reftests through WPT (Blink web_tests)

(If you want to test unlanded reftest changes to the WebGPU CTS, first check them out in
`third_party/webgpu-cts/src`, then run
`third_party/webgpu-cts/scripts/gen_ts_dep_lists.py` and
`third_party/webgpu-cts/scripts/run_regenerate_internal_cts_html.py`.)

Build the `webgpu_blink_web_tests` target (change build directory name as needed):

```sh
autoninja -C out/YOUR_TARGET webgpu_blink_web_tests
```

Then, do one of the following:

#### Manually, without expectations

- Run `third_party/blink/tools/run_blink_wptserve.py -t YOUR_TARGET`
- Open <http://localhost:8001/wpt_internal/webgpu/your_reftest.https.html> in the browser of your choice.

#### Through the automated harness, with expectations

Run tests with expectations applied (arguments copied from `test_suites.pyl`;
check there to see if this documentation is outdated):

```sh
./out/YOUR_TARGET/bin/run_webgpu_blink_web_tests --target YOUR_TARGET --flag-specific=webgpu
```

- On Linux, add:
    `--no-xvfb --additional-driver-flag=--enable-features=Vulkan`.
- For backend validation, `--flag-specific` may be changed from `webgpu` to
    `webgpu-with-backend-validation` or `webgpu-with-partial-backend-validation`.

To run a particular test rather than all the reftests, add a test filter.
Examples:
- `--isolated-script-test-filter='wpt_internal/webgpu/web_platform/reftests/canvas_complex_bgra8unorm_draw.https.html`
- `--isolated-script-test-filter='wpt_internal/webgpu/web_platform/reftests/canvas_clear.https.html`

Finally, to view the results, open `out/YOUR_TARGET/layout-test-results/results.html`.
