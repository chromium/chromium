// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels
// transferBackFromGPUTexture-disallows-transfer-back-after-draw.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      return test_canvas_disallows_transfer_back_after_draw(
          device,
          new OffscreenCanvas(50, 50));
    });
  },
  'transferBackFromGPUTexture() on a worker should not allow transferring ' +
  'back after drawing to the canvas.'
);

done();
