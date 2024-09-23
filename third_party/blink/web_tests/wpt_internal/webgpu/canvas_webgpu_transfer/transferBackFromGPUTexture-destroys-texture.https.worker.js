// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels transferBackFromGPUTexture-destroys-texture.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      return test_transferBackFromGPUTexture_destroys_texture(
          device,
          new OffscreenCanvas(50, 50),
          {});
    });
  },
  'transferBackFromGPUTexture() on a worker should destroy the associated ' +
  'GPUTexture.'
);

done();
