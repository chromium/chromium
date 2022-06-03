function repaintTest() {
    if (!window.testRunner)
        return;

    for (i = 0; i < zoomCount; ++i) {
        if (window.shouldZoomOut)
            testRunner.textZoomOut();
        else
            testRunner.textZoomIn();
    }

    testRunner.notifyDone();
}
