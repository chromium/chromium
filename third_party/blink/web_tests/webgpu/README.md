# WebGPU CTS Testing

The WebGPU conformance test suite (CTS) is developed at
<https://github.com/gpuweb/cts>. It is written in TypeScript and compiled to
JavaScript to run as part of WPT. It currently has two branches:

- `main`: Tests which do not use GLSL shaders.
- `glsl-dependent`: With additional tests that use GLSL shaders compiled to SPIR-V.

The `roll_webgpu_cts.sh` script in this directory rolls Chromium's
`third_party/webgpu-cts/src/` to the latest `glsl-dependent` revision, builds
it, and saves the built files into `.../wpt_internal/webgpu/`.
Once this is done, `.../wpt_internal/webgpu/cts.html` must also be regenerated.
This is done with the `regenerate_internal_cts_html.sh` script.
This must be done after a roll and after changes to `WebGPUExpectations`.

(Note: there is no copy of the WebGPU CTS in WPT. This is because browsers are
at different stages of implementation, and it is more useful to pin a particular
revision of the CTS rather than use the latest version.)

These scripts should work on Linux and macOS.
See the comments in those scripts for more details, and below for step-by-step
instructions on performing a roll.

## How to roll the WebGPU CTS into Chromium

1. Merge the cts `main` branch into `glsl-dependent` if it hasn't been
    done recently (or ask kainino@ to do this).
    1. Wait for
        [Chromium's mirror](https://chromium.googlesource.com/external/github.com/gpuweb/cts/+log/refs/heads/glsl-dependent)
        to pick up the changes (_usually_ &lt;10 minutes).
1. Run `third_party/blink/web_tests/webgpu/roll_webgpu_cts.sh`.
1. Repeat until regeneration succeeds:
    1. Run `third_party/blink/web_tests/webgpu/regenerate_internal_cts_html.sh`.
    1. In `third_party/blink/web_tests/WebGPUExpectations`,
        delete any expectations that caused regeneration errors
        (or rename them if you can figure out what the name change was).
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
    1. Look at output of `webgpu_blink_web_tests` on all bots
    1. Look at the output of `webgpu_blink_web_tests` (and related)
        on any failing bots.
    1. Add `WebGPUExpectations` lines for any test variants that
        "failed unexpectedly" on any tryjob.
        If they failed on all tryjobs, add them to the "Untriaged" section.
        If they failed on a specific tryjob, add them to a platform-specific section.
        1. Optionally, make some expectations more precise than
            a whole file; **re-run `regenerate_internal_cts_html.sh`
            to automatically subdivide tests to fulfill the expectations**.
        1. If a test variant times out simply because it's very long,
            add a Pass expectation for one of its immediate children
            under "Test file splits."
            (TODO: Try to figure out if we can "ping" the harness to prevent
            timeouts due to there being many CTS test cases in one WPT variant.)
    1. Run `dawn-.*-deps-rel`.
1. Get a review and land the CL!

## Testing Locally

This is not necessary for the roll process, but if you want to run a test
locally with `--enable-unsafe-webgpu`, you can easily do so here:

*   <https://gpuweb-cts-glsl.github.io/standalone/> (`glsl-dependent` branch)
*   <https://gpuweb.github.io/cts/standalone/> (`main` branch)
