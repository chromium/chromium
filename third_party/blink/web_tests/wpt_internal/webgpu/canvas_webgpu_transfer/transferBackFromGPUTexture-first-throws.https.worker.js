// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels transferBackFromGPUTexture-first-throws.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      return test_transferBackFromGPUTexture_first_throws(
          device,
          new OffscreenCanvas(50, 50));
    });
  },
  'Calling transferBackFromGPUTexture() in a worker without any preceding ' +
  'call to transferToGPUTexture() should raise an exception.'
);

done();
