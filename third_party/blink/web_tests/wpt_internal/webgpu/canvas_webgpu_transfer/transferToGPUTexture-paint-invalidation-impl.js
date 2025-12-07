/**
 * Implementation for transferToGPUTexture-paint-invalidation.
 * Draws in the canvas, then transfer it's backing texture to WebGPU. The canvas
 * should automatically get repainted with a blank content, as if newly created.
 */
async function transferToGPUTexture_paintInvalidation(canvas) {
  // Change the canvas size. This change is used in `waitForCanvasUpdate` to
  // check whether the canvas content has propagated.
  canvas.width = 100;
  canvas.height = 100;

  // First draw to the canvas and wait for the frame to be flushed.
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = 'red';
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  await new Promise((resolve) => { requestAnimationFrame(resolve) });

  // Transfer the canvas to WebGPU. The canvas element should get repainted with
  // blank content.
  const device = await getWebGPUDevice();
  ctx.transferToGPUTexture({device: device});
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
