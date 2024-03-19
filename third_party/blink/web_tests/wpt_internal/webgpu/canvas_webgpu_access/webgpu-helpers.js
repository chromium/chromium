/** Invokes `callback` with a working GPUAdapter and GPUDevice, or asserts. */
function with_webgpu(callback) {
  return navigator.gpu.requestAdapter().then((adapter) => {
    if (!(adapter instanceof GPUAdapter)) {
      assert_unreached('Failed to request WebGPU adapter.');
      return;
    }
    return adapter.requestDevice().then((device) => {
      if (!(device instanceof GPUDevice)) {
        assert_unreached('Failed to request WebGPU device.');
        return;
      }
      return callback(adapter, device);
    });
  });
}

/** getTextureFormat() should return RGBA8 or BGRA8 for a typical context. */
function test_getTextureFormat_rgba8(device, canvas) {
  const ctx = canvas.getContext('2d');
  assert_regexp_match(ctx.getTextureFormat(), /^rgba8unorm$|^bgra8unorm$/);
}

/** getTextureFormat() should return RGBA16F for a float16 context. */
function test_getTextureFormat_rgba16f(device, canvas) {
  const ctx = canvas.getContext('2d', {colorSpace: 'display-p3',
                                       pixelFormat: 'float16'});
  assert_equals(ctx.getTextureFormat(), 'rgba16float');
}
