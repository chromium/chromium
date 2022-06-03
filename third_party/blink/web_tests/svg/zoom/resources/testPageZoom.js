function repaintTest() {
    if (!window.testRunner)
        return;

    for (i = 0; i < zoomCount; ++i) {
        if (window.shouldZoomOut)
            testRunner.zoomPageOut();
        else
            testRunner.zoomPageIn();
    }

    if (!window.postZoomCallback)
        return;

    window.jsTestIsAsync = true;
    testRunner.waitUntilDone();

    window.postZoomCallback();
    finishJSTest();
}
