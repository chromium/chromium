// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels transferToGPUTexture-initialized-canvas.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      test_transferToGPUTexture_initialized_canvas(
          device,
          new OffscreenCanvas(50, 50));
    });
  },
  'transferToGPUTexture() in a worker should create a texture from an ' +
  'initialized offscreen canvas.'
);

done();
