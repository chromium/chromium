/**
 * Framework for executing tests with HTMLCanvasElement, main thread
 * OffscreenCanvas and worker OffscreenCanvas. Canvas tests are specified using
 * calls to `canvasPromiseTest`, which runs the test on the main thread, using
 * an HTML and an OffscreenCanvas. Calling `runCanvasTestsInWorker` at the
 * script level then re-execute the whole script in a worker, this time using
 * only OffscreenCanvas objects. Example usage:
 *
 * <script>
 * runCanvasTestsInWorker();
 *
 * canvasPromiseTest(async (canvas) => {
 *   // ...
 * }, "Sample test")
 * </script>
*/

/**
 * Enum listing all test types emitted by `canvasPromiseTest()`.
 */
const CanvasTestType = Object.freeze({
  HTML:   Symbol("html"),
  OFFSCREEN:  Symbol("offscreen"),
  WORKER: Symbol("worker")
});

/**
 * Run `testBody` in a `promise_test` against multiple types of canvases. By
 * default, the test is executed against an HTMLCanvasElement, a main thread
 * OffscreenCanvas and a worker OffscreenCanvas, though `testTypes` can be used
 * only enable a subset of these. `testBody` must be a function accepting a
 * canvas as parameter and returning a promise that resolves on test completion.
 *
 * This function has two implementations. The version below runs the test on the
 * main thread and another version in `canvas-worker-test.js` runs it in a
 * worker. The worker invocation is launched by calling `runCanvasTestsInWorker`
 * at the script level.
 */
function canvasPromiseTest(
    testBody, description,
    {testTypes = Object.values(CanvasTestType)} = {}) {
  setup(() => {
    const currentScript = document.currentScript;
    assert_true(
        currentScript.classList.contains('runCanvasTestsInWorkerInvoked'),
        'runCanvasTestsInWorker() must be called in the current script ' +
        'before calling canvasPromiseTest or else the test won\'t have ' +
        'worker coverage.');
  });

  if (testTypes.includes(CanvasTestType.HTML)) {
    promise_test(() => testBody(document.createElement('canvas')),
                 'HTMLCanvasElement: ' + description);
  }

  if (testTypes.includes(CanvasTestType.OFFSCREEN)) {
    promise_test(() => testBody(new OffscreenCanvas(300, 150)),
                 'OffscreenCanvas: ' + description);
  }
}

/**
 * Run all the canvasPromiseTest from the current script in a worker.
 * If the tests depend on external scripts, these must be specified as a list
 * via the `dependencies` parameter so that the worker could load them.
 */
function runCanvasTestsInWorker({dependencies = []} = {}) {
  const currentScript = document.currentScript;
  // Keep track of whether runCanvasTestsInWorker was invoked on the current
  // script. `canvasPromiseTest` will fail if `runCanvasTestsInWorker` hasn't
  // been called, to prevent accidentally omitting worker coverage.
  setup(() => {
    assert_false(
        currentScript.classList.contains('runCanvasTestsInWorkerInvoked'),
        'runCanvasTestsInWorker() can\'t be invoked twice on the same script.');
    currentScript.classList.add('runCanvasTestsInWorkerInvoked');
  });

  const canvasTests = currentScript.textContent;

  promise_setup(async () => {
    const allDeps = [
      '/resources/testharness.js',
      'canvas-worker-test.js',
    ].concat(dependencies);

    const dependencyScripts =
       await Promise.all(allDeps.map(dep => fetch(dep).then(r => r.text())));
    const allScripts = dependencyScripts.concat([canvasTests, 'done();']);

    const workerBlob = new Blob(allScripts);
    const worker = new Worker(URL.createObjectURL(workerBlob));
    fetch_tests_from_worker(worker);
  });
}
