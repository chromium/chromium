// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels beginWebGPUAccess-usage-flags.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      test_beginWebGPUAccess_usage_flags(adapter, adapterInfo, device,
                                         new OffscreenCanvas(50, 50));
    });
  },
  'beginWebGPUAccess() on a worker should create a texture which honors the ' +
  'requested usage flags.'
);

done();
