
function log(message)
{
    document.getElementById("result").innerHTML += message + "<br>";
}

var testCases = [
    "testCrossOriginLoad",
    "testCrossOriginRedirectedLoad",
];
var testIndex = 0;

function runNextTest()
{
    if (testIndex < testCases.length) {
        testIndex++;
        window[testCases[testIndex - 1]]();
    } else {
        log("DONE");
        if (window.testRunner)
            testRunner.notifyDone();
    }
}

function testCrossOriginLoad()
{
    try {
        var worker = createWorker('http://localhost:8000/workers/resources/worker-target.js');
        worker.onerror = function(evt) {
          log('SUCCESS: threw error when attempting to cross origin while loading the worker script.');
          runNextTest();
        };
        worker.onmessage = function(evt) {
          log('FAIL: executed script when redirect cross origin.');
          runNextTest();
        };
    } catch (ex) {
      log('FAIL: threw unexpected exception (' + ex +
          ') when attempting to cross origin while loading the worker script.');
      runNextTest();
    }
}

function testCrossOriginRedirectedLoad()
{
    try {
        var worker = createWorker('/resources/redirect.php?url=http://localhost:8000/workers/resources/worker-redirect-target.js');
        worker.onerror = function(evt) {
          log('SUCCESS: threw error when attempting to redirected cross origin while loading the worker script.');
          runNextTest();
        };
        worker.onmessage = function(evt) {
          log('FAIL: executed script when redirect cross origin.');
          runNextTest();
        };
    } catch (ex) {
        log("FAIL: unexpected exception " + ex);
        runNextTest();
    }
}

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

runNextTest();
