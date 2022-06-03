if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

description("Checks the various use cases around the SharedWorker constructor's optional name parameter");

var currentTest = 0;
nextTest();

// Iterates through the tests until none are left.
function nextTest()
{
    currentTest++;
    var testFunction = window["test" + currentTest];
    if (testFunction)
        testFunction();
    else
        done();
}

function test1()
{
    // Make sure we can create a shared worker with no name.
    try {
        var worker = new SharedWorker('resources/shared-worker-common.js');
        testPassed("created SharedWorker with no name");
        worker.port.postMessage("eval self.foo = 1234");
        worker.port.onmessage = function(event) {
            shouldBeEqual("setting self.foo", event.data, "self.foo = 1234: 1234");
            nextTest();
        };
    } catch (e) {
        testFailed("SharedWorker with no name threw an exception: " + e);
        done();
    }
}

function test2()
{
    // Creating a worker with no name should match an existing worker with no name
    var worker = new SharedWorker('resources/shared-worker-common.js');
    worker.port.postMessage("eval self.foo");
    worker.port.onmessage = function(event) {
        shouldBeEqual("creating worker with no name", event.data, "self.foo: 1234");
        nextTest();
    }
}

function test3()
{
    // Creating a worker with an empty name should be the same as a worker with no name.
    var worker = new SharedWorker('resources/shared-worker-common.js', '');
    worker.port.postMessage("eval self.foo");
    worker.port.onmessage = function(event) {
        shouldBeEqual("creating worker with empty name", event.data, "self.foo: 1234");
        nextTest();
    };
}

function test4()
{
    // Creating a worker with an undefined name should match an existing worker with no name.
    var worker = new SharedWorker('resources/shared-worker-common.js', undefined);
    worker.port.postMessage("eval self.foo");
    worker.port.onmessage = function(event) {
        shouldBeEqual("creating worker with an undefined name", event.data, "self.foo: 1234");
        nextTest();
    }
}

function test5()
{
    // Creating a worker with a different name should not be the same as a worker with no name.
    var worker = new SharedWorker('resources/shared-worker-common.js', 'name');
    worker.port.postMessage("eval self.foo");
    worker.port.onmessage = function(event) {
        shouldBeEqual("creating worker with different name but same URL", event.data, "self.foo: undefined");
        nextTest();
    };
}

function test6()
{
    // Creating a worker for an alternate URL with no name should work.
    var worker = new SharedWorker('resources/shared-worker-common.js?url=1');
    worker.port.postMessage("eval self.foo");
    worker.port.onmessage = function(event) {
        shouldBeEqual("creating no-name worker with alternate URL", event.data, "self.foo: undefined");
        nextTest();
    };
}

function test7()
{
    // Creating a worker for an alternate URL with empty name should work.
    var worker = new SharedWorker('resources/shared-worker-common.js?url=2', '');
    worker.port.postMessage("eval self.foo");
    worker.port.onmessage = function(event) {
        shouldBeEqual("creating empty name worker with alternate URL", event.data, "self.foo: undefined");
        nextTest();
    };
}

function test8()
{
    // Make sure we can create a shared worker with name 'null'.
    try {
        var worker = new SharedWorker('resources/shared-worker-common.js', 'null');
        testPassed("created SharedWorker with name 'null'");
        worker.port.postMessage("eval self.foo = 5678");
        worker.port.onmessage = function(event) {
            shouldBeEqual("setting self.foo", event.data, "self.foo = 5678: 5678");
            nextTest();
        };
    } catch (e) {
        testFailed("SharedWorker with name 'null' threw an exception: " + e);
        done();
    }
}

function test9()
{
    // Creating a worker with a null name should match an existing worker with no name.
    var worker = new SharedWorker('resources/shared-worker-common.js', null);
    worker.port.postMessage("eval self.foo");
    worker.port.onmessage = function(event) {
        shouldBeEqual("creating worker with a null name", event.data, "self.foo: 1234");
        nextTest();
    }
}

function test10()
{
    // Creating a worker with a specific name, the name attribute should be set to worker correctly.
    var worker = new SharedWorker('resources/shared-worker-common.js', "testingNameAttribute");
    worker.port.postMessage("testingNameAttribute");
    worker.port.onmessage = function(event) {
        shouldBeEqual("the name attribute of worker can be set correctly", event.data, "testingNameAttribute");
        nextTest();
    }
}

function shouldBeEqual(description, a, b)
{
    if (a == b)
        testPassed(description);
    else
        testFailed(description + " - passed value: " + a + ", expected value: " + b);
}
