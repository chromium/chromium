// Tests drawing while a 2D canvas is lost.
async function TestDrawWhile2dContextLost(canvas,
                                         {desynchronized = false} = {}) {
  const ctx = canvas.getContext('2d', {
    // Stay on GPU acceleration despite read-backs.
    willReadFrequently: false,
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

  // Draw a few frames while the context is lost. These should be no-op.
  for (let i = 0; i < 10; ++i) {
    ctx.fillStyle = 'blue';
    ctx.fillRect(0, 0, 100, 100);
    ctx.getImageData(0, 0, 10, 10)
    ctx.putImageData(ctx.createImageData(10, 10), 30, 30);
    await new Promise(resolve => requestAnimationFrame(resolve));
  }

  await contextRestored;

  ctx.fillStyle = 'lime';
  ctx.fillRect(0, 0, 100, 100);
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
