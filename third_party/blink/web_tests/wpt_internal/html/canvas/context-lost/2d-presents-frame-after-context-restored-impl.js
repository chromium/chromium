// Tests that a canvas can present frames to the compositor after it loses and
// restores its 2D context from a GPU process termination.
async function Test2dPresentsFrameAfterContextRestored(
    canvas, {desynchronized = false} = {}) {
  const ctx = canvas.getContext('2d', {
    desynchronized: desynchronized,
  });

  const contextLost = new Promise(resolve => {
    canvas.oncontextlost = resolve;
  });
  const contextRestored = new Promise(resolve => {
    canvas.oncontextrestored = resolve;
  });

  // Draw something and crash the GPU process.
  ctx.fillStyle = 'red';
  ctx.fillRect(0, 0, 100, 100);

  chrome.gpuBenchmarking.terminateGpuProcessNormally();

  await contextLost;
  await contextRestored;

  // Once restored, the canvas should be usable as if it's a new canvas.
  ctx.fillStyle = 'lime';
  ctx.fillRect(0, 0, 50, 50);
}

async function waitForFramePropagation(canvas) {
  const probeCanvas = new OffscreenCanvas(canvas.width, canvas.height);
  const probeCtx = probeCanvas.getContext('2d', {willReadFrequently: true});
  do {
    await new Promise(restore => requestAnimationFrame(restore));
    probeCtx.drawImage(canvas, 0, 0);
  } while (probeCtx.getImageData(1, 1, 1, 1).data[1] != 255)
  await new Promise(restore => requestAnimationFrame(restore));
}
