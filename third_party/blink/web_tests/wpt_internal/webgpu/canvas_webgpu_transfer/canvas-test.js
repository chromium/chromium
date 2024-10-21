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
 * Run `testBody` in a `promise_test`, once with an HTMLCanvasElement and once
 * more with an OffscreenCanvas. `testBody` must be a function accepting a
 * canvas as parameter and returning a promise that resolves on test completion.
 */
function canvasPromiseTest(testBody, description) {
  setup(() => {
    const currentScript = document.currentScript;
    assert_true(
        currentScript.classList.contains('runCanvasTestsInWorkerInvoked'),
        'runCanvasTestsInWorker() must be called in the current script ' +
        'before calling canvasPromiseTest or else the test won\'t have ' +
        'worker coverage.');
  });

  promise_test(() => testBody(document.createElement('canvas')),
              'HTMLCanvasElement: ' + description);

  promise_test(() => testBody(new OffscreenCanvas(300, 150)),
              'OffscreenCanvas: ' + description);
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
