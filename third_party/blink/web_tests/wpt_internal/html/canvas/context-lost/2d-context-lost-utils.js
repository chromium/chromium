/**
 * Common utilities for writing tests validating canvas 2d context loss.
 */

function get2dContext(canvas, {desynchronized = false} = {}) {
  return canvas.getContext('2d', {
    willReadFrequently: false,  // Stay on GPU acceleration despite read-backs.
    desynchronized: desynchronized
  });
};

function drawAndReadBack(ctx) {
  ctx.fillStyle = 'red';
  ctx.fillRect(0, 0, 100, 100);
  return ctx.getImageData(1, 1, 1, 1).data;
}

/**
 * Returns a promise that settles after `frameCount` animation frames.
 */
async function waitFrames(frameCount) {
  for (let i = 0; i < frameCount; ++i) {
    await Promise.race([
        new Promise(resolve => requestAnimationFrame(resolve)),
        // It's possible for `requestAnimationFrame` to never produce a frame
        // when killing the GPU process.
        promiseFinishBefore(
            20000, 'requestAnimationFrame did not produce a frame.'),
    ]);
  }
}

/**
 * Returns a promise that is rejected after `timeoutMs` milliseconds.
 * On timeout, the promise rejection will include the `errorMessage` text.
 */
function promiseFinishBefore(timeoutMs, errorMessage) {
  const { promise, reject } = Promise.withResolvers();
  setTimeout(() => reject(new Error(errorMessage)), timeoutMs);
  return promise;
}

/**
 * Binds `obj` to `promise`, to prevent `obj` from being GCed prematurely. This
 * is needed for promises waiting on contextlost or contextrestored events. If
 * the canvas is not on in the DOM, waiting on `promise` could make the whole
 * test body a floating island waiting to be GCed. The canvas event handler
 * would refer to the promise and the promise would refer to the code to resume,
 * which refers to the canvas. But if neither the canvas nor the promise is
 * anchored somewhere, there is nothing preventing the whole thing from being
 * GCed. This function anchors the island against the browser's timer, by racing
 * with a `setTimeout`, keeping everything alive at least until the timer
 * expires.
 */
function bindToPromise(obj, promise) {
  const racePromise = Promise.race([
    promise,
    // Race with a very long timeout, whose main purpose is to anchor the
    // promise in the browser's timer, preventing the promise from being GCed.
    promiseFinishBefore(20000, 'Fallback timeout should have never fired.'),
  ]);
  // Bind `obj` to `promise` to make sure `obj` won't be GCed before `promise`.
  racePromise.obj = obj;
  return racePromise;
}

/**
 * Returns a promise that resolves when the contextlost event of the canvas
 * associated with `ctx` fires.
 *
 * This function watches for contextlost events by registering an event
 * handler. That event handler is unregistered when `signal` aborts. This allows
 * racing between multiple events using `raceAndAbortOthers`.
 *
 * Of course, `promiseContextLost()` must be called before the contextlost event
 * fires. If the event already fired before calling this function, the promise
 * will never resolve. `promiseContextLost()` can still be called anytime before
 * the current task finishes though because events can't be called concurrently
 * to the current task:
 *
 *   somehowLoseTheContext();
 *   const controller = new AbortController();
 *   await promiseContextLost(controller.signal)
 *       .finally(() => controller.abort());
 *
 * If dealing with asynchronous logic however, the promise needs to be created
 * before the task that lost the context ends:
 *
 *   somehowLoseTheContext();
 *   const controller = new AbortController();
 *   const promise = promiseContextLost(ctx, controller.signal);
 *   await waitFrames(1);  // The contextlost event could fire here.
 *   await promise.finally(() => controller.abort());
 */
function promiseContextLost(ctx, signal) {
  const { promise, resolve } = Promise.withResolvers();
  ctx.canvas.addEventListener('contextlost', resolve, {signal});
  return bindToPromise(ctx, promise);
}

/**
 * Returns a promise that resolves when the contextrestored event of the canvas
 * associated with `ctx` fires.
 *
 * Behaves similarly to `promiseContextLost`, see that function for more
 * details.
 */
function promiseContextRestored(ctx, signal) {
  const { promise, resolve } = Promise.withResolvers();
  ctx.canvas.addEventListener('contextrestored', resolve, {signal});
  return bindToPromise(ctx, promise);
}

/**
 * Returns a promise that is rejected if the contextlost event of the canvas
 * associated with `ctx` fires.
 *
 * Behaves similarly to `promiseContextLost`, see that function for more
 * details.
 */
function promiseContextNotLost(ctx, signal) {
  const { promise, reject } = Promise.withResolvers();
  ctx.canvas.addEventListener(
      'contextlost',
      () => reject(new Error('Unexpected contextlost event.')),
      {signal});
  return bindToPromise(ctx, promise);
}

/**
 * Returns a promise that is rejected if the contextrestored event of the canvas
 * associated with `ctx` fires.
 *
 * Behaves similarly to `promiseContextLost`, see that function for more
 * details.
 */
function promiseContextNotRestored(ctx, signal) {
  const { promise, reject } = Promise.withResolvers();
  ctx.canvas.addEventListener(
      'contextrestored',
      () => reject(new Error('Unexpected contextrestored event.')),
      {signal});
  return bindToPromise(ctx, promise);
}

/**
 * Returns a promise that resolves or rejects when the first promise in
 * `promises` resolves or rejects. Once settled, this promise aborts
 * `controller` to clean up all passed `promises`.
 */
function raceAndAbortOthers(controller, promises) {
  return Promise.race(promises)
      .finally(() => controller.abort());
}

/**
 * Returns a promise that resolves when the canvas associated with `ctx` loses
 * it's context. The promise rejects if the contextrestored fires, or if
 * contextlost doesn't fire within `deadlineMs` milliseconds.
 *
 * Must be called before, or in the same task that loses the context or else the
 * promise will never resolve.
 */
async function waitForContextLost(ctx, deadlineMs=20000) {
  const controller = new AbortController();
  await raceAndAbortOthers(controller, [
    promiseFinishBefore(deadlineMs, 'Event contextlost should have fired.'),
    promiseContextLost(ctx, controller.signal),
    promiseContextNotRestored(ctx, controller.signal),
  ]);
}

/**
 * Returns a promise that resolves when the canvas associated with `ctx` has
 * it's context restored. The promise rejects if the contextlost fires, or if
 * contextrestored doesn't fire within `deadlineMs` milliseconds.
 *
 * Must be called before the context is restored or else the promise will never
 * resolve.
 */
async function waitForContextRestored(ctx, deadlineMs=20000) {
  const controller = new AbortController();
  await raceAndAbortOthers(controller, [
    promiseFinishBefore(deadlineMs, 'Event contextrestored should have fired.'),
    promiseContextNotLost(ctx, controller.signal),
    promiseContextRestored(ctx, controller.signal),
  ]);
}

/**
 * Returns a promise that is resolved when `promise` resolves, and is rejected
 * if the canvas associated with `ctx` gets either a contextlost or
 * contextrestored event.
 */
async function resolvedWithoutContextEvent(promise, ctx) {
  const controller = new AbortController();
  await raceAndAbortOthers(controller, [
    promise,
    promiseContextNotLost(ctx, controller.signal),
    promiseContextNotRestored(ctx, controller.signal),
  ]);
}

/**
 * Returns a promise that resolves after `frameCount` animation frames and
 * rejects if a contextlost or contextrestored event fires before then.
 */
async function waitFramesAndRejectOnContextEvents(ctx, frameCount=5) {
  return resolvedWithoutContextEvent(waitFrames(frameCount), ctx);
}

/**
 * Returns a promise that is resolved when the GPU process goes back online.
 * This must be called before the GPU process dies and awaited on after.
 */
function promiseGpuProcessRestored() {
  const probeCanvas = document.createElement('canvas');
  const probeCtx = get2dContext(probeCanvas);
  drawAndReadBack(probeCtx);
  return promiseContextRestored(probeCtx);
}

/**
 * Terminates the GPU process and return a promise that resolves when the GPU
 * process is back online. A cleanup callback is registered into the provided
 * `test` fixture to make sure that the GPU process is restored before the test
 * completes.
 */
function terminateGpuProcess(test) {
  const gpuRestoredPromise = promiseGpuProcessRestored();
  test.add_cleanup(() => gpuRestoredPromise);
  chrome.gpuBenchmarking.terminateGpuProcessNormally();
  return gpuRestoredPromise;
}
