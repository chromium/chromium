/** Worker script needed by canvas-test.js. */

/**
 * Worker version of `canvasPromiseTest()`, running `testBody` with an
 * OffscreenCanvas. `testBody` must be a function accepting a canvas as
 * parameter and returning a promise that resolves on test completion.
 */
function canvasPromiseTest(testBody, description) {
  promise_test(() => testBody(new OffscreenCanvas(300, 150)),
              'Worker: ' + description);
}

/**
 * The function `runCanvasTestsInWorker()` in `canvas-test.js` re-executes the
 * current script in a worker. That script inevitably contain the call to
 * `runCanvasTestsInWorker()`, which triggered the whole thing. For that call
 * to succeed, the worker must have a definition for that function. There's
 * nothing to do here though, the script is already running in a worker.
 */
function runCanvasTestsInWorker() {}
