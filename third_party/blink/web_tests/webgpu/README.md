# WebGPU CTS Testing

The WebGPU conformance test suite (CTS) is developed at
<https://github.com/gpuweb/cts>. It is written in TypeScript and compiled to
JavaScript to run as part of WPT.

(Note: there is no copy of the WebGPU CTS in WPT. This is because browsers are
at different stages of implementation, and it is more useful to pin a particular
revision of the CTS rather than use the latest version.)

An autoroller (https://autoroll.skia.org/r/webgpu-cts-chromium-autoroll) rolls the WebGPU
CTS into Chromium regularly. Part of the roll requires regenerating a few files which the
autoroller attempts to do.
1. `third_party/webgpu-cts/ts_sources.txt` is a generated file which tells GN the list of Typescript
   sources that may impact compilation.
1. `third_party/blink/web_tests/wpt_internal/webgpu/cts.html` is a generated file for WPT and
   contains all of the "variants" the CTS is run with. It is generated with the script
   `third_party/webgpu-cts/scripts/run_regenerate_internal_cts_html.py` based on a manual list of
   tests to split into variants (`third_party/blink/web_tests/webgpu/internal_cts_test_splits.pyl`)
   and `third_party/blink/web_tests/WebGPUExpectations`.

Should the autoroller fail, a manual roll is required.
See below for step-by-step instructions on performing a roll.

## How to manually roll the WebGPU CTS into Chromium

1. Run `roll-dep --roll-to origin/main src/third_party/webgpu-cts/src`. This will produce a commit
   that updates DEPS.
1. Run `third_party/webgpu-cts/scripts/gen_ts_dep_lists.py`, add any changes, and amend the previous
   commit. GN requires us to include a list of all Typescript sources that will affect compilation.
1. Repeat until regeneration succeeds:
    1. Run `third_party/webgpu-cts/scripts/run_regenerate_internal_cts_html.py`.
    1. In `third_party/blink/web_tests/WebGPUExpectations`,
        delete any expectations that caused regeneration errors
        (or try to update them if there was a rename).
1. Commit changes, upload patch (ignore line-length warnings in generated files).
1. Run these tryjobs: `dawn-.*-deps-rel`.
1. Make sure there isn't anything terribly wrong
    (e.g. a harness bug that causes all tests to fail, or not run at all).
1. Remove stale expectations:
    1. Look at the output of `webgpu_blink_web_tests` (and related)
        on those tryjobs.
    1. Remove any expectations for "passed unexpectedly" test variants
        in `WebGPUExpectations`.
1. Repeat until CQ passes:
    1. Look at output of `webgpu_blink_web_tests` on all bots.
    1. Look at the output of `webgpu_blink_web_tests` (and related)
        on any failing bots.
    1. Add `WebGPUExpectations` lines for any test variants that
        "failed unexpectedly" on any tryjob.
        If they failed on all tryjobs, add them to the "Untriaged" section.
        If they failed on a specific tryjob, add them to a platform-specific section.
        1. Fail and Skip expectations may be any valid WebGPU CTS test query. Other expectations
           like Slow, Crash, and Timeout must list parameters in the same exact order that the
           test runner loads them in. You can check the ordering by looking at the test code, or
           by loading the standalone CTS runner in Chrome.
        1. If using a Slow, Crash, or Timeout expectation that is more precise than a whole variant,
            it is necessary to **re-run `run_regenerate_internal_cts_html.py`
            to automatically subdivide tests to fulfill the expectations**.
        1. If a test variant times out simply because it's very long,
            add a test query to `third_party/blink/web_tests/webgpu/internal_cts_test_splits.pyl`
            for one of its immediate children and re-run `run_regenerate_internal_cts_html.py`.
            (TODO: Try to figure out if we can "ping" the harness to prevent
            timeouts due to there being many CTS test cases in one WPT variant.)
    1. Run `dawn-.*-deps-rel`.
1. Get a review and land the CL!

## Testing Locally

This is not necessary for the roll process, but if you want to run a test
locally with `--enable-unsafe-webgpu`, you can easily do so here:

*   <https://gpuweb.github.io/cts/standalone/>
