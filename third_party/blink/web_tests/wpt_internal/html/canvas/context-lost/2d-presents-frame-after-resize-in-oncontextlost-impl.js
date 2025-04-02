// Tests resizing a 2d canvas from within the oncontextlost event.
async function TestResizeInOnContextLost(canvas,
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

  ctx.fillStyle = 'red';
  ctx.fillRect(0, 0, 100, 100);

  chrome.gpuBenchmarking.terminateGpuProcessNormally();
  await contextLost;
  // Resize from within the oncontextlost task.
  canvas.width = 100;
  await contextRestored;

  ctx.fillStyle = 'lime';
  ctx.fillRect(0, 0, 50, 50);
}

async function waitForFramePropagation(canvas) {
  const probeCanvas = new OffscreenCanvas(canvas.width, canvas.height);
  const probeCtx = probeCanvas.getContext('2d', {willReadFrequently: true});
  do {
    await new Promise(restore => requestAnimationFrame(restore));
    probeCtx.drawImage(canvas, 0, 0);
  } while (canvas.width != 100 ||
           probeCtx.getImageData(1, 1, 1, 1).data[1] != 255)
  await new Promise(restore => requestAnimationFrame(restore));
}
