assert_true(!!window.chrome && !!chrome.gpuBenchmarking,
  'This test requires chrome.gpuBenchmarking.');

// Test that a canvas loses and restores a 2D context after the GPU process is
// terminated.
async function Test2dContextLostAndRestored(test, canvas,
                                            {desynchronized = false} = {}) {
  const ctx = get2dContext(canvas, {desynchronized});

  // Draw something and crash the GPU process.
  ctx.fillStyle = 'red';
  ctx.fillRect(0, 0, 100, 100);

  terminateGpuProcess(test);

  assert_false(ctx.isContextLost());
  await waitForContextLost(ctx);
  assert_true(ctx.isContextLost());

  // The canvas should remain blank until it's restored.
  assert_array_equals(
      drawAndReadBack(ctx, 'lime'), [0, 0, 0, 0],
      'The canvas should remain blank while the context is lost.');

  assert_true(ctx.isContextLost());
  await waitForContextRestored(ctx);
  assert_false(ctx.isContextLost());

  // Once restored, the canvas should be usable as if it's a new canvas.
  assert_array_equals(
      ctx.getImageData(2, 2, 1, 1).data, [0, 0, 0, 0],
      `The canvas should be blank right after it's restored.`);

  assert_array_equals(
      drawAndReadBack(ctx, 'lime'), [0, 255, 0, 255],
      `The canvas should be usable after it's restored.`);
}

// Tests that the canvas is not lost after the GPU process is terminated.
async function Test2dContextNeverLost(test, canvas,
                                      {desynchronized = false} = {}) {
  const ctx = get2dContext(canvas, {desynchronized});

  // Draw something and crash the GPU process.
  ctx.fillStyle = 'red';
  ctx.fillRect(0, 0, 100, 100);

  const gpuProcessRestoredPromise = terminateGpuProcess(test);

  // The canvas should still be alive.
  assert_false(ctx.isContextLost());
  assert_array_equals(
      ctx.getImageData(2, 2, 1, 1).data, [255, 0, 0, 255],
      'The canvas should still be healthy after the GPU process died.');

  await resolvedWithoutContextEvent(gpuProcessRestoredPromise, ctx);

  assert_false(ctx.isContextLost());
  assert_array_equals(
    ctx.getImageData(2, 2, 1, 1).data, [255, 0, 0, 255],
    'The canvas should still be healthy a while after the GPU process died.');
}
