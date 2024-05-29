// META: global=worker

// ============================================================================

importScripts("/resources/testharness.js");
importScripts("./webgpu-helpers.js");

// This test parallels transferToWebGPU-unbalanced-access.https.html.
promise_test(() => {
    return with_webgpu((adapter, adapterInfo, device) => {
      return test_transferToWebGPU_unbalanced_access(
          adapterInfo,
          device,
          new OffscreenCanvas(50, 50));
    });
  },
  'Unbalanced calls to transferToWebGPU() in a worker will destroy the old ' +
  'WebGPU access texture.'
);

done();
