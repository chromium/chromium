/**
 * Returns a promise resolving to a GPUAdapter.
 */
async function getWebGPUAdapter() {
  const adapter = await navigator.gpu.requestAdapter();
  if (!(adapter instanceof GPUAdapter)) {
    throw new Error('Failed to request WebGPU adapter.');
  }
  return adapter;
}

/**
 * Returns a promise resolving to a GPUDevice. Requests the GPUDevice from
 * `adapter` if specified, else, requests from a new adapter.
 */
async function getWebGPUDevice(adapter) {
  adapter = adapter || await getWebGPUAdapter();

  const device = await adapter.requestDevice();
  if (!(device instanceof GPUDevice)) {
    throw new Error('Failed to request WebGPU device.');
  }
  return device;
}

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
 * Tests that a transfer in one canvas either does or does not result in an
 * initial transfer in a second canvas being zero-copy depending on whether the
 * canvases are in-DOM canvas elements or offscreen canvases.
 */
function test_transferToGPUTexture_two_canvases(device, canvas1, canvas2,
                                                is_canvas_element) {
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
  // WebGPU. This ensures that the test below is meaningful.
  const ctx2 = canvas2.getContext('2d');
  const w2 = ctx2.canvas.width;
  const h2 = ctx2.canvas.height;
  ctx2.fillStyle = "#00FF00";
  ctx2.fillRect(0, 0, w2, h2 / 2);

  if (is_canvas_element) {
    // An initial transfer on the second canvas element should not incur a copy.
    ctx1.transferToGPUTexture({device: device, requireZeroCopy: true});
    ctx1.transferBackFromGPUTexture();
  } else {
    // An initial transfer on the second offscreen canvas must incur a copy.
    try {
      ctx2.transferToGPUTexture({device: device, requireZeroCopy: true});
      assert_unreached('transferToGPUTexture should have thrown.');
    } catch (ex) {
      assert_true(ex instanceof DOMException);
      assert_equals(ex.name, 'InvalidStateError');
      assert_true(
          ex.message.includes('Transferring canvas to GPU was not zero-copy'));
    }
  }
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
