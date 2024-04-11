// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels endWebGPUAccess-destroys-texture.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      return test_endWebGPUAccess_destroys_texture(
          device,
          new OffscreenCanvas(50, 50),
          {});
    });
  },
  'endWebGPUAccess() on a worker should destroy the associated GPUTexture.'
);

done();
