// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels transferToGPUTexture-usage-flags.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      test_transferToGPUTexture_usage_flags(adapter, adapterInfo, device,
                                         new OffscreenCanvas(50, 50));
    });
  },
  'transferToGPUTexture() on a worker should create a texture which honors ' +
  'the requested usage flags.'
);

done();
