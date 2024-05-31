// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels canvas-reset-destroys-texture.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      return test_canvas_reset_destroys_texture(
          adapterInfo,
          device,
          new OffscreenCanvas(50, 50),
          'resize');
    });
  },
  'Resizing a canvas during WebGPU access on a worker destroys the GPUTexture.'
);

promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      return test_canvas_reset_destroys_texture(
          adapterInfo,
          device,
          new OffscreenCanvas(50, 50),
          'api');
    });
  },
  'Calling context.reset() during WebGPU access on a worker destroys the ' +
  'GPUTexture.'
);

done();
