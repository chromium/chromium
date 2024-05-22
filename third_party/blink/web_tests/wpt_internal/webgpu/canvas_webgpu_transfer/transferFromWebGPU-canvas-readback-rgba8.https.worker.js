// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels transferFromWebGPU-canvas-readback.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      return test_transferFromWebGPU_canvas_readback(
          adapterInfo,
          device,
          new OffscreenCanvas(50, 50),
          {});
    });
  },
  'transferFromWebGPU() should preserve texture changes when called from a ' +
  'worker.'
);

done();
