// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels transferToWebGPU-balanced-access.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      test_transferToWebGPU_balanced_access(
          device,
          new OffscreenCanvas(50, 50));
    });
  },
  'transferToWebGPU() in a worker allows repeated calls after a call to ' +
  'transferBackFromWebGPU().'
);

done();
