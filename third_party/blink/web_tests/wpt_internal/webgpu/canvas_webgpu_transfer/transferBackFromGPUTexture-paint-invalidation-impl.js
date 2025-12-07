/**
 * Implementation for transferBackFromGPUTexture-paint-invalidation.
 * Draws in the canvas, then overwrite content via WebGPU. The canvas should
 * automatically get repainted with the new content.
 */
async function transferBackFromGPUTexture_paintInvalidation(canvas) {
  // Change the canvas size. This change is used in `waitForCanvasUpdate` to
  // check whether the canvas content has propagated.
  canvas.width = 100;
  canvas.height = 100;

  // First draw to the canvas and wait for the frame to be flushed.
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = 'red';
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  await new Promise((resolve) => { requestAnimationFrame(resolve) });

  // Replace content via WebGPU. The canvas element should get repainted with
  // new content.
  const device = await getWebGPUDevice();
  const texture = ctx.transferToGPUTexture({device: device});
  clearTextureToColor(device, texture,
                      { r: 64 / 255, g: 128 / 255, b: 192 / 255, a: 1.0 });
  ctx.transferBackFromGPUTexture();
}

/**
 * Wait until the changes from `transferBackFromGPUTexture_paintInvalidation`
 * propagated from the OffscreenCanvas to the specified placerholder `canvas`.
 */
async function waitForCanvasUpdate(canvas) {
  while (canvas.width != 100) {
    await new Promise(resolve => requestAnimationFrame(resolve));
  }
  await new Promise(resolve => requestAnimationFrame(resolve));
}
