if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.setDumpConsoleMessages(false);
    testRunner.dumpChildFrames();
    testRunner.waitUntilDone();
}

window.onload = function() {
    var frame = document.getElementById('aFrame');
    frame.onload = runNextTest;
    runNextTest();
}

var testURIs = [
    'javascript:alert("FAIL: this should not have been loaded.")',
    ' javascript:alert("FAIL: this should not have been loaded.")',
    'javascript\t:alert("FAIL: this should not have been loaded.")',
    'java\0script:alert("FAIL: this should not have been loaded.")',
    'javascript\1:alert("FAIL: this should not have been loaded.")',
    'http://localhost:8000/security/resources/cross-frame-iframe.html'
];

var currentTestIndex = 0;
function runNextTest() {
    testRunner.logToStderr("runNextTest: " + currentTestIndex);
    if (currentTestIndex == testURIs.length) {
        if (window.testRunner)
            testRunner.notifyDone();
        return;
    }

    var frame = document.getElementById('aFrame');
    var uri = testURIs[currentTestIndex];
    try {
        setter(frame, uri);
    } catch (e) {
        console.log("FAIL: Unexpected exception: '" + e.message + "'.");
    }

    currentTestIndex++;
    if (currentTestIndex <= 3) {
        // First 3 uris will be silently ignored / will not get onload event.
        // Therefore - we need to kick off the next test ourselves.
        runNextTest();
    }
}
