// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels transferToWebGPU-unbalanced-access.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      test_transferToWebGPU_unbalanced_access(
          device,
          new OffscreenCanvas(50, 50));
    });
  },
  'transferToWebGPU() in a worker disallows repeated calls without a call ' +
  'to transferBackFromWebGPU().'
);

done();
