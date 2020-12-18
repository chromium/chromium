// Run a callback after a frame update.
//
// Note that this file has two copies:
//   resources/run-after-layout-and-paint.js
// and
//   http/tests/resources/run-after-layout-and-paint.js.
// They should be kept always the same.
//
// The function runAfterLayoutAndPaint() has two modes:
// - traditional mode, for existing tests, and tests needing customized
//   notifyDone timing:
//     if (window.testRunner)
//       testRunner.waitUntilDone();
//     runAfterLayoutAndPaint(function() {
//       ... // some code which modifies style/layout
//       if (window.testRunner)
//         testRunner.notifyDone();
//       // Or notifyDone any time later if needed.
//     });
//
// - autoNotifyDone mode, for new tests which just need to change style/layout
//   and finish:
//     runAfterLayoutAndPaint(function() {
//       ... // some code which modifies style/layout
//     }, true);
//
// Note that because we always update a frame before finishing a test,
// we don't need
//     runAfterLayoutAndPaint(function() { testRunner.notifyDone(); })
// to ensure the test finish after a frame update.
//
if (window.internals)
    internals.runtimeFlags.paintUnderInvalidationCheckingEnabled = true;

function runAfterLayoutAndPaint(callback, autoNotifyDone) {
    if (!window.testRunner) {
        // For manual test. Delay 500ms to allow us to see the visual change
        // caused by the callback.
        setTimeout(callback, 500);
        return;
    }

    if (autoNotifyDone)
        testRunner.waitUntilDone();

    // We do requestAnimationFrame and setTimeout to ensure a frame has started
    // and layout and paint have run. The requestAnimationFrame fires after the
    // frame has started but before layout and paint. The setTimeout fires
    // at the beginning of the next frame, meaning that the previous frame has
    // completed layout and paint.
    // See http://crrev.com/c/1395193/10/third_party/blink/web_tests/http/tests/resources/run-after-layout-and-paint.js
    // for more discussions.
    requestAnimationFrame(function() {
        setTimeout(function() {
            callback();
            if (autoNotifyDone)
                testRunner.notifyDone();
        }, 1);
    });
}

function test_after_layout_and_paint(func, name, properties) {
    var test = async_test(name, properties);
    runAfterLayoutAndPaint(test.step_func(() => {
        func.call(test, test);
        test.done();
    }, false));
}

function async_test_after_layout_and_paint(func, name, properties) {
    var test = async_test(name, properties);
    runAfterLayoutAndPaint(test.step_func(() => {
        func.call(test, test);
    }, false));
}
