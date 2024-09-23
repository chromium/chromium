// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test should be kept in sync with getTextureFormat-rgba8.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      test_getTextureFormat_rgba8(device, new OffscreenCanvas(50, 50));
    });
  }, 'getTextureFormat() returns RGBA8 or BGRA8 for a worker context'
);

// This test should be kept in sync with getTextureFormat-rgba16f.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      test_getTextureFormat_rgba16f(device, new OffscreenCanvas(50, 50));
    });
  }, 'getTextureFormat() returns RGBA16F for a float16 worker context'
);

done();
