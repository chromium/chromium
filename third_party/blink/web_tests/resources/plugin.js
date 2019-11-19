// Ensure plugin load, run the speicified function, then finish the test.
function startAfterLoadAndFinish(f, node) {
    if (window.testRunner)
        testRunner.waitUntilDone();
    window.addEventListener('load', function() {
        if (window.internals)
            internals.updateLayoutAndRunPostLayoutTasks(node);
        if (f)
            f();
        testRunner.notifyDone();
    }, false);
}
