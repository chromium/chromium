"use strict";

/**
 * Tests body for canvas getCurrentTexture expiry
 *
 * @param {GPUCanvasContext | OffscreenRenderingContext} ctx the WebGPU context
 * @param {GPUDevice} device the WebGPU device
 * @param {string} prevFrameCallsite = {'runInNewCanvasFrame', 'requestAnimationFrame'}
 * @param {boolean} getCurrentTextureAgain = {true, false}
 */
async function test(ctx, device, prevFrameCallsite, getCurrentTextureAgain) {
  const promises = [];

  const canvasType = ctx instanceof GPUCanvasContext ? 'onscreen' : 'offscreen';

  // The fn is called immediately after previous frame updating the rendering.
  // Polyfill by calling the callback by setTimeout, in the requestAnimationFrame callback (for onscreen canvas)
  // or after transferToImageBitmap (for offscreen canvas).
  function runInNewCanvasFrame(fn) {
    switch (canvasType) {
      case 'onscreen':
        requestAnimationFrame(() => setTimeout(fn));
        break;
      case 'offscreen':
        // for offscreen canvas, after calling transferToImageBitmap, we are in a new frame immediately
        ctx.canvas.transferToImageBitmap();
        fn();
        break;
      default:
        assert_unreached();
        break;
    }
  }

  function checkGetCurrentTexture() {
    // Call getCurrentTexture on previous frame.
    const prevTexture = ctx.getCurrentTexture();

    promises.push(new Promise(resolve => {
      // Call getCurrentTexture immediately after the frame, the texture object should stay the same.
      queueMicrotask(() => {
        if (getCurrentTextureAgain) {
          assert_true(prevTexture === ctx.getCurrentTexture());
        }
        resolve();
      });
    }));

    promises.push(new Promise(resolve => {
      // Call getCurrentTexture immediately after this frame updating the rendering.
      // We want chromium to expire the prevTexture and return a new texture object as early as the next task.
      setTimeout(async () => {
          if (getCurrentTextureAgain) {
            assert_true(prevTexture !== ctx.getCurrentTexture());
          }

          // Event when prevTexture expired, createView should still succeed anyway.
          const prevTextureView = prevTexture.createView();
          const bgl = device.createBindGroupLayout({
            entries: [
              {
                binding: 0,
                visibility: GPUShaderStage.COMPUTE,
                texture: {},
              },
            ],
          });

          device.pushErrorScope('validation');

          // Using the invalid texture view should fail if it expires.
          device.createBindGroup({
            layout: bgl,
            entries: [{ binding: 0, resource: prevTextureView }],
          });

          const promise = device.popErrorScope();
          const gpuValidationError = await promise;
          assert_true(gpuValidationError instanceof GPUValidationError);

          resolve();
        });
    }));
  }

  switch (prevFrameCallsite) {
    case 'runInNewCanvasFrame':
      runInNewCanvasFrame(checkGetCurrentTexture);
      break;
    case 'requestAnimationFrame':
      requestAnimationFrame(checkGetCurrentTexture);
      break;
    default:
      assert_unreached();
      break;
  }

  await Promise.all(promises);
}
