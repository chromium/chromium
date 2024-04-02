// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels beginWebGPUAccess-texture-readback.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      return test_beginWebGPUAccess_texture_readback(
          device,
          new OffscreenCanvas(50, 50));
    });
  },
  'beginWebGPUAccess() texture retains the contents of the offscreen canvas, ' +
  'and readback works, from a worker.'
);

done();
