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
    return adapter.requestDevice().then((device) => {
      if (!(device instanceof GPUDevice)) {
        assert_unreached('Failed to request WebGPU device.');
        return;
      }
      return callback(adapter, adapter.info, device);
    });
  });
}

/** Returns true if we are running on a Mac with a SwiftShader GPU adapter. */
function isMacSwiftShader(adapterInfo) {
  assert_true(adapterInfo instanceof GPUAdapterInfo);
  return adapterInfo.architecture == 'swiftshader' &&
         navigator.platform.startsWith('Mac');
}

/** Returns true if we are running on Linux. */
function isLinux() {
  return navigator.platform.startsWith('Linux');
}

/** Returns the upper-left hand pixel from the texture in a buffer. */
function copyOnePixelFromTextureAndSubmit(device, tex) {
  // Make a buffer which will allow us to copy one pixel from the texture, and
  // later read that pixel back.
  const buf = device.createBuffer({usage: GPUBufferUsage.COPY_DST |
                                          GPUBufferUsage.MAP_READ,
                                   size: 4});

  // Copy one pixel out of our texture.
  const encoder = device.createCommandEncoder();
  encoder.copyTextureToBuffer({texture: tex}, {buffer: buf}, [1, 1]);
  device.queue.submit([encoder.finish()]);

  // Return the buffer in case the caller wants to use it.
  return buf;
}

/** Replaces the upper-left hand pixel in the texture with transparent black. */
function copyOnePixelToTextureAndSubmit(device, tex) {
  // Make a buffer which will allow us to copy one pixel to the texture.
  const buf = device.createBuffer({usage: GPUBufferUsage.COPY_SRC, size: 4});

  // Copy a pixel into the upper-left pixel of our texture. The buffer is never
  // initialized, so it's going to default to transparent black (all zeros).
  const encoder = device.createCommandEncoder();
  encoder.copyBufferToTexture({buffer: buf}, {texture: tex}, [1, 1]);
  device.queue.submit([encoder.finish()]);
}

/** Using the passed-in texture as a render attachment, clears the texture. */
function clearTextureToColor(device, tex, gpuColor) {
  const encoder = device.createCommandEncoder();
  const pass = encoder.beginRenderPass({
    colorAttachments: [{
       view: tex.createView(),
       loadOp: 'clear',
       storeOp: 'store',
       clearValue: gpuColor,
    }]
  });
  pass.end();
  device.queue.submit([encoder.finish()]);
}

/** Verifies that every pixel on a Canvas2D contains a particular RGBA color. */
function checkCanvasColor(ctx, expectedRGBA) {
  // The ImageData `data` array holds RGBA elements in uint8 format.
  const w = ctx.canvas.width;
  const h = ctx.canvas.height;
  const imageData = ctx.getImageData(0, 0, w, h);
  for (let idx = 0; idx < w * h * 4; idx += 4) {
    assert_array_equals([imageData.data[idx + 0],
                         imageData.data[idx + 1],
                         imageData.data[idx + 2],
                         imageData.data[idx + 3]],
                        expectedRGBA,
                        'RGBA @ ' + idx);
  }
}

/**
 * Returns true if the texture causes validation errors when it is read-from or
 * written-to. This is indicative of a destroyed texture.
 */
async function isTextureDestroyed(device, texture) {
  // Unfortunately, there isn't a simple way to test if a texture has been
  // destroyed or not in WebGPU. The best we can do is try to use the texture in
  // various ways and check for GPUValidationErrors. So we verify whether or not
  // we are able to read-from or write-to the texture. If we get GPU validation
  // errors for both, we consider it to be destroyed.
  device.pushErrorScope('validation');
  copyOnePixelFromTextureAndSubmit(device, texture);

  device.pushErrorScope('validation');
  copyOnePixelToTextureAndSubmit(device, texture);

  writeError = await device.popErrorScope();
  readError = await device.popErrorScope();

  return readError instanceof GPUValidationError &&
         writeError instanceof GPUValidationError;
}

/**
 * Creates a bind group which uses the passed-in texture as a resource.
 * This will cause a GPUValidationError if TEXTURE_BINDING usage isn't set.
 * The caller is responsible for pushing and popping an error scope.
 */
function createBindGroupUsingTexture(device, tex) {
  const layout = device.createBindGroupLayout({
    entries: [
      {
        binding: 1,
        visibility: GPUShaderStage.FRAGMENT,
        texture: {},
      },
    ],
  });

  const group = device.createBindGroup({
    layout: layout,
    entries: [
      {
        binding: 1,
        resource: tex.createView(),
      },
    ],
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

/**
 * transferToGPUTexture() should create a texture from an uninitialized canvas.
 */
function test_transferToGPUTexture_untouched_canvas(device, canvas) {
  // Leave the canvas untouched; it won't have a Canvas2DLayerBridge yet.

  // Next, call transferToGPUTexture.
  const ctx = canvas.getContext('2d');
  const tex = ctx.transferToGPUTexture({device: device,
                                         label: "hello, webgpu!"});

  // Confirm that we now have a GPU texture that matches our request.
  assert_true(tex instanceof GPUTexture, 'not a GPUTexture');
  assert_equals(tex.label, "hello, webgpu!");
  assert_equals(tex.width, canvas.width);
  assert_equals(tex.height, canvas.height);
}

/**
 * transferToGPUTexture() should create a texture from an initialized canvas.
 */
function test_transferToGPUTexture_initialized_canvas(device, canvas) {
  // Paint into the canvas to ensure it has a valid Canvas2DLayerBridge.
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = "#FFFFFF";
  ctx.fillRect(0, 0, 100, 50);

  // Next, call transferToGPUTexture.
  const tex = ctx.transferToGPUTexture({device: device,
                                         label: "hello, webgpu!"});

  // Confirm that we now have a GPU texture that matches our request.
  assert_true(tex instanceof GPUTexture, 'not a GPUTexture');
  assert_equals(tex.label, 'hello, webgpu!');
  assert_equals(tex.width, canvas.width);
  assert_equals(tex.height, canvas.height);
}

/**
 * transferToGPUTexture() texture should retain the contents of the canvas, and
 * readback works. Returns a promise.
 */
function test_transferToGPUTexture_texture_readback(device, canvas) {
  // Fill the canvas with a color containing distinct values in each channel.
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = "#4080C0";
  ctx.fillRect(0, 0, 50, 50);

  // Convert the canvas to a texture.
  const tex = ctx.transferToGPUTexture({device: device,
                                     usage: GPUTextureUsage.COPY_SRC});

  // Copy the top-left pixel from the texture into a buffer.
  const buf = copyOnePixelFromTextureAndSubmit(device, tex);

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
 * Calling transferBackFromGPUTexture() without any preceding call to
 * transferToGPUTexture should raise an exception.
 */
function test_transferBackFromGPUTexture_first_throws(device, canvas) {
  const ctx = canvas.getContext('2d');

  try {
    ctx.transferBackFromGPUTexture();
    assert_unreached('transferBackFromGPUTexture should have thrown.');
  } catch (ex) {
    assert_true(ex instanceof DOMException);
    assert_equals(ex.name, 'InvalidStateError');
  }
}

/**
 * Unbalanced calls to transferToGPUTexture() will destroy the old WebGPU access
 * texture.
 */
async function test_transferToGPUTexture_unbalanced_access(
      adapterInfo, device, canvas) {
  // Skip this test on Mac Swiftshader.
  if (isMacSwiftShader(adapterInfo)) {
    return;
  }

  // Begin a WebGPU access session.
  const ctx = canvas.getContext('2d');
  const tex1 = ctx.transferToGPUTexture({device: device,
                                      usage: GPUTextureUsage.COPY_DST |
                                             GPUTextureUsage.COPY_SRC});

  // Start a second WebGPU access session, destroying the first texture.
  const tex2 = ctx.transferToGPUTexture({device: device,
                                      usage: GPUTextureUsage.COPY_DST |
                                             GPUTextureUsage.COPY_SRC});

  // Only the second texture should remain in an undestroyed state.
  assert_true(await isTextureDestroyed(device, tex1));
  assert_false(await isTextureDestroyed(device, tex2));
}

/**
 * transferToGPUTexture() should allow repeated calls after a call to
 * transferBackFromGPUTexture().
 */
function test_transferToGPUTexture_balanced_access(device, canvas) {
  const ctx = canvas.getContext('2d');

  // Draw to the canvas via the canvas2D API to ensure that the SharedImage
  // backing the canvas is created before doing any transfers to WebGPU. This
  // ensures that this SharedImage will be created without WebGPU usage, which
  // the first test below assumes as a precondition.
  const w = ctx.canvas.width;
  const h = ctx.canvas.height;
  ctx.fillStyle = "#00FF00";
  ctx.fillRect(0, 0, w, h / 2);

  // An initial transfer incurs a copy as the canvas resource's SharedImage
  // doesn't have WebGPU usage by default. Validate that `requireZeroCopy` is
  // getting forwarded properly by verifying that passing `true` causes an
  // exception to be raised.
  try {
    const tex = ctx.transferToGPUTexture({device: device, requireZeroCopy: true});
    assert_unreached('transferToGPUTexture should have thrown.');
  } catch (ex) {
    assert_true(ex instanceof DOMException);
    assert_equals(ex.name, 'InvalidStateError');
    assert_true(ex.message.includes('Transferring canvas to GPU was not zero-copy'));
  }

  // Begin and end a WebGPU access session several times. No further transfers
  // on this canvas should incur a copy.
  for (let count = 0; count < 10; ++count) {
    const tex = ctx.transferToGPUTexture({device: device, requireZeroCopy: true});
    ctx.transferBackFromGPUTexture();
  }
}

/**
 * Tests that a transfer in one canvas does not result in an initial transfer in
 * a second canvas being zero-copy.
 */
function test_transferToGPUTexture_two_canvases(device, canvas1, canvas2) {
  const ctx1 = canvas1.getContext('2d');

  // Draw to the first canvas via the canvas2D API to ensure that the SharedImage
  // backing this canvas is created before doing any transfers to WebGPU. This
  // ensures that this SharedImage will be created without WebGPU usage, which
  // the test below assumes as a precondition.
  const w1 = ctx1.canvas.width;
  const h1 = ctx1.canvas.height;
  ctx1.fillStyle = "#00FF00";
  ctx1.fillRect(0, 0, w1, h1 / 2);

  // An initial transfer incurs a copy as the canvas resource's SharedImage
  // doesn't have WebGPU usage by default. Validate that `requireZeroCopy` is
  // getting forwarded properly by verifying that passing `true` causes an
  // exception to be raised.
  try {
    const tex = ctx1.transferToGPUTexture({device: device, requireZeroCopy: true});
    assert_unreached('transferToGPUTexture should have thrown.');
  } catch (ex) {
    assert_true(ex instanceof DOMException);
    assert_equals(ex.name, 'InvalidStateError');
    assert_true(ex.message.includes('Transferring canvas to GPU was not zero-copy'));
  }

  // A second transfer on the same canvas should not incur a copy.
  {
    const tex = ctx1.transferToGPUTexture({device: device, requireZeroCopy: true});
    ctx1.transferBackFromGPUTexture();
  }

  // Draw to the second canvas via the canvas2D API to ensure that the
  // SharedImage backing this canvas is created before doing any transfers to
  // WebGPU. This ensures that this SharedImage will be created without WebGPU
  // usage, which the test below assumes as a precondition.
  const ctx2 = canvas2.getContext('2d');
  const w2 = ctx2.canvas.width;
  const h2 = ctx2.canvas.height;
  ctx2.fillStyle = "#00FF00";
  ctx2.fillRect(0, 0, w2, h2 / 2);

  // An initial transfer on the second canvas must incur a copy.
  try {
    const tex = ctx2.transferToGPUTexture({device: device, requireZeroCopy: true});
    assert_unreached('transferToGPUTexture should have thrown.');
  } catch (ex) {
    assert_true(ex instanceof DOMException);
    assert_equals(ex.name, 'InvalidStateError');
    assert_true(ex.message.includes('Transferring canvas to GPU was not zero-copy'));
  }
}

/**
 * transferBackFromGPUTexture() should preserve texture changes on the 2D
 * canvas.
 */
function test_transferBackFromGPUTexture_canvas_readback(adapterInfo, device,
                                                     canvas, canvasFormat) {
  // Skip this test on Mac Swiftshader due to "Invalid Texture" errors.
  if (isMacSwiftShader(adapterInfo)) {
    return;
  }

  // Convert the canvas to a texture.
  const ctx = canvas.getContext('2d', canvasFormat);
  const tex = ctx.transferToGPUTexture({device: device});

  // Fill the texture with a color containing distinct values in each channel.
  clearTextureToColor(device, tex,
                      { r: 64 / 255, g: 128 / 255, b: 192 / 255, a: 1.0 });

  // Finish our WebGPU pass and restore the canvas.
  ctx.transferBackFromGPUTexture(tex);

  // Verify that the canvas contains our chosen color across every pixel.
  checkCanvasColor(ctx, [0x40, 0x80, 0xC0, 0xFF]);
}

/**
 * transferBackFromGPUTexture() should be a no-op if the canvas context is lost.
 */
function test_transferBackFromGPUTexture_context_lost(device, canvas) {
  // Begin a WebGPU access session.
  const ctx = canvas.getContext('2d');
  const tex = ctx.transferToGPUTexture({device: device});

  // Forcibly lose the canvas context.
  assert_true(!!window.internals, 'Internal APIs unavailable.');
  internals.forceLoseCanvasContext(canvas, '2d');

  // End the WebGPU access session. Nothing should be thrown.
  try {
    ctx.transferBackFromGPUTexture(tex);
  } catch {
    assert_unreached('transferBackFromGPUTexture should be safe when context ' +
                     'is lost.');
  }
}

/**
 * transferBackFromGPUTexture() should cause the GPUTexture returned by
 * transferToGPUTexture() to enter a destroyed state.
 */
async function test_transferBackFromGPUTexture_destroys_texture(
    device, canvas) {
  // Briefly begin a WebGPU access session.
  const ctx = canvas.getContext('2d');
  const tex = ctx.transferToGPUTexture({device: device,
                                     usage: GPUTextureUsage.COPY_SRC |
                                            GPUTextureUsage.COPY_DST});
  ctx.transferBackFromGPUTexture();

  // `tex` should be in a destroyed state.
  assert_true(await isTextureDestroyed(device, tex));
}

/** Resizing a canvas during WebGPU access should destroy the texture. */
async function test_canvas_reset_destroys_texture(adapterInfo, device, canvas,
                                                  resetType) {
  // Skip this test on Mac Swiftshader due to "Invalid Texture" errors.
  if (isMacSwiftShader(adapterInfo)) {
    return;
  }

  // Begin a WebGPU access session.
  const ctx = canvas.getContext('2d');
  const tex = ctx.transferToGPUTexture({device: device,
                                     usage: GPUTextureUsage.COPY_SRC});

  // Reset the canvas. This should abort the WebGPU access session.
  if (resetType == 'resize') {
    canvas.width = canvas.width;
  } else {
    assert_equals(resetType, 'api');
    ctx.reset();
  }

  // The canvas' GPUTexture should appear to be destroyed.
  assert_true(await isTextureDestroyed(device, tex));
}

/**
 * transferToGPUTexture() should create a texture which honors the requested
 * usage flags.
 */
function test_transferToGPUTexture_usage_flags(adapter, adapterInfo, device,
                                           canvas) {
  // Create a base-case promise that just resolves immediately.
  let promise = new Promise((resolve, reject) => { resolve(true); });

  // Skip this test on Mac Swiftshader due to "Invalid Texture" errors.
  if (isMacSwiftShader(adapterInfo)) {
    return promise;
  }

  // Declare a helper function which tests individual usage flags.
  const testOneUsageFlag = function(device, canvas, usageToEnable,
                                    usageToTest) {
    const ctx = canvas.getContext('2d');
    const tex = ctx.transferToGPUTexture({device: device,
                                           usage: usageToEnable});

    // Check that the GPUTexture's usage flags match our requested flags.
    assert_equals(tex.usage, usageToEnable);

    // Confirm that we _actually_ have the requested usage by performing an
    // action requiring that usage, and verifying no GPUValidationError occurs.
    device.pushErrorScope('validation');

    if (usageToTest & GPUTextureUsage.COPY_SRC) {
      copyOnePixelFromTextureAndSubmit(device, tex);
    }
    if (usageToTest & GPUTextureUsage.COPY_DST) {
      copyOnePixelToTextureAndSubmit(device, tex);
    }
    if (usageToTest & GPUTextureUsage.TEXTURE_BINDING) {
      createBindGroupUsingTexture(device, tex);
    }
    if (usageToTest & GPUTextureUsage.RENDER_ATTACHMENT) {
      clearTextureToColor(device, tex, { r: 1.0, g: 1.0, b: 1.0, a: 1.0 });
    }

    return device.popErrorScope().then((errors) => {
      if (usageToTest == usageToEnable) {
        assert_equals(errors, null, 'enabled ' + usageToEnable +
                                    ', tested ' + usageToTest);
      } else {
        assert_not_equals(errors, null, 'enabled ' + usageToEnable +
                                        ', tested ' + usageToTest);
      }
      ctx.transferBackFromGPUTexture();
    });
  };

  // Build up a chain of promises which test each TextureUsage flag.
  const supportedUsageFlags = [GPUTextureUsage.COPY_SRC,
                               GPUTextureUsage.COPY_DST,
                               GPUTextureUsage.TEXTURE_BINDING,
                               GPUTextureUsage.RENDER_ATTACHMENT];

  for (const usageToEnable of supportedUsageFlags) {
    for (const usageToTest of supportedUsageFlags) {
      promise = promise.then(() => {
        return with_webgpu((adapter, adapterInfo, device) => {
          return testOneUsageFlag(device, canvas, usageToEnable, usageToTest);
        });
      });
    }
  }

  return promise;
}

/**
 * WebGPU access should work normally on a canvas which has been downgraded to
 * CPU, transparently bringing it back to the GPU.
 */
function test_canvas_works_after_cpu_downgrade(adapterInfo, device, canvas) {
  const ctx = canvas.getContext('2d');

  // Force the canvas into software.
  internals.disableCanvasAcceleration(canvas);

  // Perform a GPU clear to demonstrate that the canvas still works in WebGPU.
  const tex = ctx.transferToGPUTexture({device: device});
  clearTextureToColor(device, tex, { r: 0.0, g: 1.0, b: 0.0, a: 1.0 });
  ctx.transferBackFromGPUTexture();

  // Verify that WebGPU did its job; every pixel on the canvas should be green.
  // TODO(crbug.com/40218893): Linux with an Intel GPU returns the wrong color.
  if (!isLinux()) {
    checkCanvasColor(ctx, [0x00, 0xFF, 0x00, 0xFF]);
  }
}

/**
 * WebGPU access should not allow transferring back after drawing to the canvas.
 */
function test_canvas_disallows_transfer_back_after_draw(device, canvas) {
  const ctx = canvas.getContext('2d');

  // Transfer the GPU texture off the context and clear it to red.
  // This red is never transferred back, so it doesn't matter.
  const tex = ctx.transferToGPUTexture({device: device});
  clearTextureToColor(device, tex, { r: 1.0, g: 0.0, b: 0.0, a: 1.0 });

  // Draw a green rectangle using Canvas2D commands. Only cover the top half of
  // the canvas. The bottom half will be untouched by drawing commands. Note
  // that there should be no red pixels, despite the above call to
  // `clearTextureToColor`; the canvas should be treated as newly-initialized.
  const w = ctx.canvas.width;
  const h = ctx.canvas.height;
  ctx.fillStyle = "#00FF00";
  ctx.fillRect(0, 0, w, h / 2);

  // Attempting to transfer the GPU texture back should raise an exception.
  try {
    ctx.transferBackFromGPUTexture();
    assert_unreached('transferBackFromGPUTexture should have thrown.');
  } catch (ex) {
    assert_true(ex instanceof DOMException);
    assert_equals(ex.name, 'InvalidStateError');
  }

  // The canvas' image should contain the rectangle, and should have zero red
  // or blue pixels.
  const imageData = ctx.getImageData(0, 0, w, h);
  for (let idx = 0; idx < w * h * 4; idx += 4) {
    assert_equals(imageData.data[idx + 0], 0x00,
                  'Expected no red in the canvas image.');
    assert_equals(imageData.data[idx + 2], 0x00,
                  'Expected no blue in the canvas image.');
  }

  // We should be able to find some green pixels from the rectangle.
  let foundGreen = false;
  for (let idx = 0; idx < w * h * 4; idx += 4) {
    if (imageData.data[idx + 1] == 0xFF) {
      foundGreen = true;
      break;
    }
  }
  assert_true(foundGreen, 'Expected some green in the canvas image.');
}
