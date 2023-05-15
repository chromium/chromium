# WebGPU canvas related tests for Chromium specific behavior

These standalone wpt_internal web_tests are similar to their WebGPU CTS counterparts under `src/webgpu/web_platform/canvas` but test Chromium specific behaviors. These tests are also easier to test different combinations of canvas type, how HTML update rendering happens, in main thread or worker etc.

* canvas-get-current-texture-expiry-*.https.html: mostly testing the same things as the CTS tests at https://github.com/gpuweb/cts/pull/2386 with some variations to reflect Chromium specific behavior. So that we can have stricter requirement for how Chromium getCurrentTexture behaves.
  - TODO:
    - if on a different thread, expiry happens when the worker updates its rendering (worker "rPAF") OR transferToImageBitmap is called
    - [draw, transferControlToOffscreen, then canvas is displayed] on either {main thread, or transferred to worker}
    - [draw, canvas is displayed, then transferControlToOffscreen] on either {main thread, or transferred to worker}
    - reftests for the above 2 (what gets displayed when the canvas is displayed)
    - with canvas element added to DOM or not (applies to other canvas tests as well)
        - canvas is added to DOM after being rendered
        - canvas is already in DOM but becomes visible after being rendered
