// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels transferToWebGPU-initialized-canvas.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      test_transferToWebGPU_initialized_canvas(
          device,
          new OffscreenCanvas(50, 50));
    });
  },
  'transferToWebGPU() in a worker should create a texture from an ' +
  'initialized offscreen canvas.'
);

done();
