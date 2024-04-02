// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels endWebGPUAccess-canvas-readback.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      return test_endWebGPUAccess_canvas_readback(
          adapterInfo,
          device,
          new OffscreenCanvas(50, 50),
          {colorSpace: 'srgb', pixelFormat: 'float16'});
    });
  },
  'endWebGPUAccess() should preserve texture changes on an RGBA16F canvas ' +
  'when called from a worker.'
);

done();
