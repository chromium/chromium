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

/** beginWebGPUAccess() should create a texture from an uninitialized canvas. */
function test_beginWebGPUAccess_untouched_canvas(device, canvas) {
  // Leave the canvas untouched; it won't have a Canvas2DLayerBridge yet.

  // Next, call beginWebGPUAccess.
  const ctx = canvas.getContext('2d');
  const tex = ctx.beginWebGPUAccess({device: device, label: "hello, webgpu!"});

  // Confirm that we now have a GPU texture that matches our request.
  assert_true(tex instanceof GPUTexture, 'not a GPUTexture');
  assert_equals(tex.label, "hello, webgpu!");
  assert_equals(tex.width, ctx.canvas.width);
  assert_equals(tex.height, ctx.canvas.height);
}

/** beginWebGPUAccess() should create a texture from an initialized canvas. */
function test_beginWebGPUAccess_initialized_canvas(device, canvas) {
  // Paint into the canvas to ensure it has a valid Canvas2DLayerBridge.
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = "#FFFFFF";
  ctx.fillRect(0, 0, 100, 50);

  // Next, call beginWebGPUAccess.
  const tex = ctx.beginWebGPUAccess({device: device, label: "hello, webgpu!"});

  // Confirm that we now have a GPU texture that matches our request.
  assert_true(tex instanceof GPUTexture, 'not a GPUTexture');
  assert_equals(tex.label, 'hello, webgpu!');
  assert_equals(tex.width, ctx.canvas.width);
  assert_equals(tex.height, ctx.canvas.height);
}

/**
 * beginWebGPUAccess() texture should retain the contents of the canvas, and
 * readback works. Returns a promise.
 */
function test_beginWebGPUAccess_texture_readback(device, canvas) {
  // Fill the canvas with a color containing distinct values in each channel.
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = "#4080C0";
  ctx.fillRect(0, 0, 50, 50);

  // Convert the canvas to a texture.
  const tex = ctx.beginWebGPUAccess({device: device});

  // Make a buffer for reading back one pixel from the texture.
  const buf = device.createBuffer({usage: GPUBufferUsage.COPY_DST |
                                          GPUBufferUsage.MAP_READ,
                                    size: 4});

  // Copy the top-left pixel from the texture into a buffer.
  const encoder = device.createCommandEncoder();
  encoder.copyTextureToBuffer({texture: tex}, {buffer: buf}, [1, 1]);
  device.queue.submit([encoder.finish()]);

  // Map the buffer and read it back.
  return buf.mapAsync(GPUMapMode.READ).then(() => {
    const data = new Uint8Array(buf.getMappedRange());

    if (tex.format == 'rgba8unorm') {
      assert_array_equals(data, [64, 128, 192, 255]);
    } else {
      assert_equals(tex.format, 'bgra8unorm');
      assert_array_equals(data, [192, 128, 64, 255]);
    }
  });
};

/**
 * beginWebGPUAccess() disallows repeated calls without a call to
 * endWebGPUAccess().
 */
function test_beginWebGPUAccess_unbalanced_access(device, canvas) {
  // Begin a WebGPU access session.
  const ctx = canvas.getContext('2d');
  const tex = ctx.beginWebGPUAccess({device: device});

  try {
    // Try to start a second WebGPU access session.
    tex = ctx.beginWebGPUAccess({device: device});
    assert_unreached('InvalidStateError should have been thrown');
  } catch (ex) {
    assert_true(ex instanceof DOMException);
    assert_equals(ex.name, 'InvalidStateError');
  }
}

/**
 * beginWebGPUAccess() should allow repeated calls after a call to
 * endWebGPUAccess().
 */
function test_beginWebGPUAccess_balanced_access(device, canvas) {
  const ctx = canvas.getContext('2d');

  // Begin and end a WebGPU access session several times.
  for (let count = 0; count < 10; ++count) {
    const tex = ctx.beginWebGPUAccess({device: device});
    ctx.endWebGPUAccess();
  }
}
