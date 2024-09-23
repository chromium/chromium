// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels transferToGPUTexture-untouched-canvas.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      test_transferToGPUTexture_untouched_canvas(
          device,
          new OffscreenCanvas(50, 50));
    });
  },
  'transferToGPUTexture() in a worker should create a texture from an ' +
  'uninitialized canvas.'
);

done();
