assert_true(!!window.chrome && !!chrome.gpuBenchmarking,
  'This test requires chrome.gpuBenchmarking.');

// Test that a canvas loses and restores a 2D context after the GPU process is
// terminated.
async function Test2dContextLostAndRestored(canvas,
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

  assert_false(ctx.isContextLost());
  await contextLost;
  assert_true(ctx.isContextLost());

  // The canvas should remain blank until it's restored.
  ctx.fillStyle = 'lime';
  ctx.fillRect(0, 0, 100, 100);
  assert_array_equals(
      ctx.getImageData(2, 2, 1, 1).data, [0, 0, 0, 0],
      'The canvas should remain blank while the context is lost.');

  assert_true(ctx.isContextLost());
  await contextRestored;
  assert_false(ctx.isContextLost());

  // Once restored, the canvas should be usable as if it's a new canvas.
  assert_array_equals(
      ctx.getImageData(2, 2, 1, 1).data, [0, 0, 0, 0],
      `The canvas should be blank right after it's restored.`);

  ctx.fillStyle = 'lime';
  ctx.fillRect(0, 0, 100, 100);
  assert_array_equals(
      ctx.getImageData(2, 2, 1, 1).data, [0, 255, 0, 255],
      `The canvas should be usable after it's restored.`);
}

// Tests that the canvas is not lost after the GPU process is terminated.
async function Test2dContextNeverLost(t, canvas,
                                      {desynchronized = false} = {}) {
  const ctx = canvas.getContext('2d', {
    // Stay on GPU acceleration despite read-backs.
    willReadFrequently: false,
    desynchronized: desynchronized,
  });

  canvas.oncontextlost = t.step_func(() => {
    assert_unreached('The context should not have been lost.');
  });

  // Draw something and crash the GPU process.
  ctx.fillStyle = 'red';
  ctx.fillRect(0, 0, 100, 100);

  chrome.gpuBenchmarking.terminateGpuProcessNormally();

  // The canvas should still be alive.
  assert_false(ctx.isContextLost());
  assert_array_equals(
      ctx.getImageData(2, 2, 1, 1).data, [255, 0, 0, 255],
      'The canvas should still be healthy after the GPU process died.');

  // Wait for a few frames and check that the canvas is still healthy.
  for (let i = 0; i < 10; ++i) {
    await new Promise(resolve => requestAnimationFrame(resolve));
  }
  assert_false(ctx.isContextLost());
  assert_array_equals(
    ctx.getImageData(2, 2, 1, 1).data, [255, 0, 0, 255],
    'The canvas should still be healthy a while after the GPU process died.');
}
