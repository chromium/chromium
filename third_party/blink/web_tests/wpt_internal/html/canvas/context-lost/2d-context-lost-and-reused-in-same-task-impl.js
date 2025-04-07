assert_true(!!window.chrome && !!chrome.gpuBenchmarking,
  'This test requires chrome.gpuBenchmarking.');

/**
 * Test losing the GPU context and keep using the canvas in the same task,
 * before the canvas realizes that the context is lost.
 */
async function TestLosingAndReusingCanvasInSameTask(canvas) {
  const ctx = canvas.getContext('2d',
                                // Stay on GPU acceleration despite read-backs.
                                {willReadFrequently: false});

  // Draw in the next frame to make sure a cc::Layer is created.
  await new Promise(resolve => requestAnimationFrame(resolve));
  ctx.fillStyle = 'red';
  ctx.fillRect(0, 0, 100, 100);

  // Lose context in the next frame.
  await new Promise(resolve => requestAnimationFrame(resolve));
  chrome.gpuBenchmarking.terminateGpuProcessNormally();

  // Reading back an accelerated canvas requires a roundtrip to the GPU process,
  // causing the implementation to realize it's no longer there.
  ctx.getImageData(2, 2, 1, 1);

  // The canvas is still unaware that the context is lost. Drawing to a GPU
  // canvas is no-op regardless. Drawing to a software canvas still works.
  assert_false(ctx.isContextLost(),
               'The canvas context should not yet be lost.');
  ctx.fillStyle = 'blue';
  ctx.fillRect(0, 0, 100, 100);
  if (internals.runtimeFlags.accelerated2dCanvasEnabled) {
    assert_array_equals(
        ctx.getImageData(2, 2, 1, 1).data, [0, 0, 0, 0],
        'Draw calls should be no-ops after the GPU process dies.');
  } else {
    assert_array_equals(
        ctx.getImageData(2, 2, 1, 1).data, [0, 0, 255, 255],
        'Software canvas should still be usable after the GPU process dies.');
  }

  // Check that the context usable again after it's restored.
  await new Promise(resolve => { canvas.oncontextrestored = resolve; });

  ctx.fillStyle = 'lime';
  ctx.fillRect(0, 0, 100, 100);
  assert_array_equals(
      ctx.getImageData(2, 2, 1, 1).data, [0, 255, 0, 255],
      `The canvas should be usable after it's restored.`);
}
