// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test should be kept in sync with getTextureFormat-rgba16f.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      test_getTextureFormat_rgba16f(device, new OffscreenCanvas(50, 50));
    });
  }, 'getTextureFormat() returns RGBA16F for a float16 worker context'
);

done();
