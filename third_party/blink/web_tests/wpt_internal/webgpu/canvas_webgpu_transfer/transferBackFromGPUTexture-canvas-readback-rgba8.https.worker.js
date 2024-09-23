// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels transferBackFromGPUTexture-canvas-readback.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      return test_transferBackFromGPUTexture_canvas_readback(
          adapterInfo,
          device,
          new OffscreenCanvas(50, 50),
          {});
    });
  },
  'transferBackFromGPUTexture() should preserve texture changes when called from a ' +
  'worker.'
);

done();
