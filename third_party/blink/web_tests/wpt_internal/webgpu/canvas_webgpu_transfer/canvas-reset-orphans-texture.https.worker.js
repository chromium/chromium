// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels canvas-reset-orphans-texture.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      test_canvas_reset_orphans_texture(
          adapterInfo,
          device,
          new OffscreenCanvas(50, 50),
          'resize');
    });
  },
  'Resizing a canvas during WebGPU access on a worker orphans the GPUTexture.'
);

promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      test_canvas_reset_orphans_texture(
          adapterInfo,
          device,
          new OffscreenCanvas(50, 50),
          'api');
    });
  },
  'Calling context.reset() during WebGPU access on a worker orphans the ' +
  'GPUTexture.'
);

done();
