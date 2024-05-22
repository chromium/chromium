// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels transferFromWebGPU-exchanges-texture.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      return test_transferFromWebGPU_exchanges_texture(
                 device,
                 new OffscreenCanvas(50, 50),
                 new OffscreenCanvas(50, 50));
    });
  },
  'transferFromWebGPU() on a worker should allow canvases to exchange textures.'
);

done();
