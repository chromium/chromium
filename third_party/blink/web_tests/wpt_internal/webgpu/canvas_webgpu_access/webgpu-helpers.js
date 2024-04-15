/**
 * Invokes `callback` with a valid GPUAdapter, GPUAdapterInfo and GPUDevice, or
 * causes an assertion.
 */
function with_webgpu(callback) {
  return navigator.gpu.requestAdapter().then((adapter) => {
    if (!(adapter instanceof GPUAdapter)) {
      assert_unreached('Failed to request WebGPU adapter.');
      return;
    }
    return adapter.requestAdapterInfo().then((adapterInfo) => {
      if (!(adapterInfo instanceof GPUAdapterInfo)) {
        assert_unreached('Failed to request WebGPU adapter info.');
        return;
      }
      return adapter.requestDevice().then((device) => {
        if (!(device instanceof GPUDevice)) {
          assert_unreached('Failed to request WebGPU device.');
          return;
        }
        return callback(adapter, adapterInfo, device);
      });
    });
  });
}

/** Returns true if we are running on a Mac with a SwiftShader GPU adapter. */
function isMacSwiftShader(adapterInfo) {
  assert_true(adapterInfo instanceof GPUAdapterInfo);
  return adapterInfo.architecture == 'swiftshader' &&
         navigator.platform.startsWith('Mac');
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
  assert_equals(tex.width, canvas.width);
  assert_equals(tex.height, canvas.height);
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
  assert_equals(tex.width, canvas.width);
  assert_equals(tex.height, canvas.height);
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
    assert_unreached('InvalidStateError should have been thrown.');
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

/** endWebGPUAccess() should preserve texture changes on the 2D canvas. */
function test_endWebGPUAccess_canvas_readback(adapterInfo, device,
                                              canvas, canvasFormat) {
  // Skip this test on Mac Swiftshader due to "Invalid Texture" errors.
  if (isMacSwiftShader(adapterInfo)) {
    return;
  }

  // Convert the canvas to a texture.
  const ctx = canvas.getContext('2d', canvasFormat);
  const tex = ctx.beginWebGPUAccess({device: device});

  // Fill the texture with a color containing distinct values in each channel.
  const encoder = device.createCommandEncoder();
  const pass = encoder.beginRenderPass({
    colorAttachments: [{
       view: tex.createView(),
       loadOp: 'clear',
       storeOp: 'store',
       clearValue: { r: 64 / 255, g: 128 / 255, b: 192 / 255, a: 1.0 },
    }]
  });
  pass.end();
  device.queue.submit([encoder.finish()]);

  // Finish our WebGPU pass and restore the canvas.
  ctx.endWebGPUAccess();

  // Verify that the canvas contains our chosen color across every pixel.
  // The ImageData `data` array holds RGBA elements in uint8 format.
  const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
  for (let idx = 0; idx < canvas.width * canvas.height * 4; idx += 4) {
    assert_array_equals([imageData.data[idx + 0],
                         imageData.data[idx + 1],
                         imageData.data[idx + 2],
                         imageData.data[idx + 3]],
                        [0x40, 0x80, 0xC0, 0xFF],
                        'RGBA @ ' + idx);
  }
}

/** endWebGPUAccess() should be a no-op if the canvas context is lost. */
function test_endWebGPUAccess_context_lost(device, canvas) {
  // Begin a WebGPU access session.
  const ctx = canvas.getContext('2d');
  const tex = ctx.beginWebGPUAccess({device: device});

  // Forcibly lose the canvas context.
  assert_true(!!window.internals, 'Internal APIs unavailable.');
  internals.forceLoseCanvasContext(canvas, '2d');

  // End the WebGPU access session. Nothing should be thrown.
  try {
    ctx.endWebGPUAccess();
  } catch {
    assert_unreached('endWebGPUAccess should be safe when context is lost.');
  }
}

/**
 * endWebGPUAccess() should cause the GPUTexture returned by beginWebGPUAccess()
 * to enter a destroyed state.
 */
function test_endWebGPUAccess_destroys_texture(device, canvas) {
  // Briefly begin a WebGPU access session.
  const ctx = canvas.getContext('2d');
  const tex = ctx.beginWebGPUAccess({device: device});
  ctx.endWebGPUAccess();

  // Make a buffer which will allow us to copy one pixel to or from the texture.
  const buf = device.createBuffer({usage: GPUBufferUsage.COPY_SRC |
                                          GPUBufferUsage.COPY_DST,
                                    size: 4});

  // `tex` should be in a destroyed state. Unfortunately, there isn't a
  // foolproof way to test for this state in WebGPU. The best we can do is try
  // to use the texture in various ways and check for GPUValidationErrors.
  // So we verify that we are not able to read-from or write-to the texture.
  device.pushErrorScope('validation');
  const texToBufEncoder = device.createCommandEncoder();
  texToBufEncoder.copyTextureToBuffer({texture: tex}, {buffer: buf}, [1, 1]);
  device.queue.submit([texToBufEncoder.finish()]);

  device.pushErrorScope('validation');
  const bufToTexEncoder = device.createCommandEncoder();
  bufToTexEncoder.copyBufferToTexture({buffer: buf}, {texture: tex}, [1, 1]);
  device.queue.submit([bufToTexEncoder.finish()]);

  return device.popErrorScope().then((writeError) => {
    return device.popErrorScope().then((readError) => {
      assert_true(readError instanceof GPUValidationError);
      assert_true(writeError instanceof GPUValidationError);
    });
  });
}

/** Resizing a canvas during WebGPU access should throw an exception. */
function test_canvas_reset_orphans_texture(adapterInfo, device, canvas,
                                           resetType) {
  // Skip this test on Mac Swiftshader due to "Invalid Texture" errors.
  if (isMacSwiftShader(adapterInfo)) {
    return;
  }

  // Begin a WebGPU access session.
  const ctx = canvas.getContext('2d');
  const tex = ctx.beginWebGPUAccess({device: device});

  // Reset the canvas. This should abort the WebGPU access session. The canvas'
  // GPUTexture is still accessible from Javascript.
  if (resetType == 'resize') {
    canvas.width = canvas.width;
  } else {
    assert_equals(resetType, 'api');
    ctx.reset();
  }

  // Verify that the WebGPU access was terminated by starting a new WebGPU
  // access session. This would throw if the initial beginWebGPUAccess session
  // were still active.
  ctx.beginWebGPUAccess({device: device});
  ctx.endWebGPUAccess();

  // Verify that the texture from the initial WebGPU access session is still
  // usable by accessing its contents. This would cause a GPUValidationError if
  // the texture had been destroyed, invalidated, or reabsorbed into the canvas.
  device.pushErrorScope('validation');

  const buf = device.createBuffer({usage: GPUBufferUsage.COPY_DST, size: 4});
  const texToBufEncoder = device.createCommandEncoder();
  texToBufEncoder.copyTextureToBuffer({texture: tex}, {buffer: buf}, [1, 1]);
  device.queue.submit([texToBufEncoder.finish()]);

  return device.popErrorScope().then((errors) => {
    assert_equals(errors, null);
  });
}
