// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels transferToGPUTexture-two-canvases.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      test_transferToGPUTexture_two_canvases(
          device,
          new OffscreenCanvas(50, 50),
          new OffscreenCanvas(50, 50));
    });
  },
  'A transfer in one offscreen canvas in a worker does not result in an ' +
  'initial transfer in a second offscreen canvas in that worker being ' +
  'zero-copy.'
);

done();
