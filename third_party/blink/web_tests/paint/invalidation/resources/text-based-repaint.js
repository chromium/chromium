// Asynchronous tests should manually call finishRepaintTest at the appropriate
// time.
window.testIsAsync = false;
window.outputRepaintRects = true;

// All repaint tests are asynchronous.
if (window.testRunner)
    testRunner.waitUntilDone();

if (window.internals)
    internals.runtimeFlags.paintUnderInvalidationCheckingEnabled = true;

// Add string names of objects that should be invalidated here. If you use this feature,
// you must also include testharness.js.
window.expectedObjectInvalidations = [];
// Objects which must *not* be invalidated.
window.expectedObjectNonInvalidations = [];

function runRepaintTest()
{
    if (!window.testRunner || !window.internals) {
        setTimeout(repaintTest, 500);
        return;
    }

    if (window.enablePixelTesting)
        testRunner.dumpAsTextWithPixelResults();
    else
        testRunner.dumpAsText();

    // This is equivalent to runAfterLayoutAndPaint() in
    // ../../resources/run-after-layout-and-paint.js. Duplicate it here so that
    // the callers don't need to include that file.
    requestAnimationFrame(() => {
        setTimeout(() => {
            internals.startTrackingRepaints(top.document);
            repaintTest();
            if (!window.testIsAsync)
                finishRepaintTest();
        }, 0);
    });
}

function runRepaintAndPixelTest()
{
    window.enablePixelTesting = true;
    runRepaintTest();
}

function finishRepaintTest()
{
    if (!window.testRunner || !window.internals)
        return;

    var flags = internals.LAYER_TREE_INCLUDES_INVALIDATIONS;

    if (window.layerTreeAsTextAdditionalFlags)
        flags |= layerTreeAsTextAdditionalFlags;

    var layersWithInvalidationsText = internals.layerTreeAsText(top.document, flags);

    internals.stopTrackingRepaints(top.document);

    // Play nice with JS tests which may want to print out assert results.
    if (window.isJsTest)
        window.outputRepaintRects = false;

    if (window.outputRepaintRects)
        testRunner.setCustomTextOutput(layersWithInvalidationsText);

    if (window.afterTest)
        window.afterTest();

    // Play nice with async JS tests which want to notifyDone themselves.
    if (!window.jsTestIsAsync)
        testRunner.notifyDone();
}
