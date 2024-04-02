// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels beginWebGPUAccess-initialized-canvas.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      test_beginWebGPUAccess_initialized_canvas(
          device,
          new OffscreenCanvas(50, 50));
    });
  },
  'beginWebGPUAccess() in a worker should create a texture from an ' +
  'initialized offscreen canvas.'
);

done();
