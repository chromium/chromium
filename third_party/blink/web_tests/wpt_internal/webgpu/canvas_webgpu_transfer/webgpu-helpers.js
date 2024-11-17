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
