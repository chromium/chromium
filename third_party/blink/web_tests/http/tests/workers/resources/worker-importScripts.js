if (self.postMessage)
    runTests();
 else
    onconnect = handleConnect;

function handleConnect(event)
{
    // For shared workers, create a faux postMessage() API to send messages back to the parent page.
    self.postMessage = function(message) { event.ports[0].postMessage(message); }
    runTests();
}

function runTests()
{
    try {
        postMessage("Test started.");

        importScripts();
        postMessage("PASS: importScripts(), exists, is a function, and doesn't throw when not given any arguments");

        var source1 = "worker-importScripts-source1.js";
        var source2 = "worker-importScripts-source2.js";
        var differentOrigin = "http://localhost:8000/workers/resources/worker-importScripts-differentOrigin.js";
        var differentRedirectOrigin = "/resources/redirect.php?url=http://localhost:8000/workers/resources/worker-importScripts-differentOrigin.js";
        var syntaxErrorSource = "worker-importScripts-syntaxError.js";
        var fakeSource = "nonexistant";
        loadedSource1 = false;
        loadedSource2 = false;
        differentOriginLoaded = false;

        function resetLoadFlags() {
            loadedSource1 = false;
            loadedSource2 = false;
            differentOriginLoaded = false;
        }

        try {
            importScripts(differentOrigin)
        } catch (e) {
            postMessage("FAIL: Threw " + e + " when attempting cross origin load");
        }
        if (differentOriginLoaded)
            postMessage("PASS: executed script from different origin");

        resetLoadFlags();

        try {
            importScripts(differentRedirectOrigin)
        } catch (e) {
            postMessage("FAIL: Threw " + e + " when attempting load from different origin through a redirect");
        }
        if (differentOriginLoaded)
            postMessage("PASS: executed script from different origin through a redirect");
        else
            postMessage("FAIL: did not load script from different origin through a redirect");

        resetLoadFlags();

        postMessage("Testing single argument:");
        importScripts(source1)
        if (loadedSource1)
            postMessage("PASS: loaded first source");
        else
            postMessage("FAIL: did not load first source");

        resetLoadFlags();

        postMessage("Testing multiple arguments:");
        importScripts(source1, source2);
        if (loadedSource1 && loadedSource2)
            postMessage("PASS: Both sources loaded and executed.");
        else
            postMessage("FAIL: not all sources were loaded");

        resetLoadFlags();

        postMessage("Testing multiple arguments (different order):");
        importScripts(source2, source1);
        if (loadedSource1 && loadedSource2)
            postMessage("PASS: Both sources loaded and executed.");
        else
            postMessage("FAIL: not all sources were loaded");

        resetLoadFlags();
        firstShouldThrow = false;
        secondShouldThrow = false;
        postMessage("Testing multiple arguments, with different origin for one argument:");
        try {
            importScripts(source1, differentOrigin, source2);
        } catch (e) {
            postMessage("FAIL: Threw " + e + " when attempt cross origin load");
        }
        if (loadedSource1 && loadedSource2 && differentOriginLoaded)
            postMessage("PASS: all resources executed.");
        else
            postMessage("FAIL: Not  of the origin failure");

        resetLoadFlags();

        try {
            importScripts(source1, fakeSource, source2);
        } catch (e) {
            postMessage("PASS: Threw " + e + " when load failed");
        }
        if (!loadedSource1 && !loadedSource2)
            postMessage("FAIL: Nothing was executed when network error occurred.");
        else
            postMessage("PASS: some resources were loaded despite the network error");

        resetLoadFlags();

        try {
            importScripts(source1, syntaxErrorSource, source2);
        } catch (e) {
            postMessage("PASS: Threw " + e + " when encountering a syntax error in imported script");
        }
        if (!loadedSource1 && !loadedSource2)
            postMessage("FAIL: Nothing was executed when syntax error was present in any source.");
        else
            postMessage("PASS: some resources were loaded despite the presence of a syntax error");

        resetLoadFlags();

        postMessage("Testing multiple arguments, with first resource throwing an exception:");
        firstShouldThrow = true;
        try {
            importScripts(source1, source2);
        } catch (e) {
            postMessage("PASS: Propagated '" + e + "' from script");
        }
        firstShouldThrow = false;
        if (loadedSource1 && !loadedSource2)
            postMessage("PASS: First resource was executed, and second resource was not");
        else if (loadedSource2)
            postMessage("FAIL: Second resource was executed");
        else
            postMessage("FAIL: first resource did not execute correctly");

        resetLoadFlags();

        postMessage("Testing multiple arguments, with second resource throwing an exception:");
        secondShouldThrow = true;
        try {
            importScripts(source1, source2);
        } catch (e) {
            postMessage("PASS: Propagated '" + e + "' from script");
        }
        secondShouldThrow = false;
        if (loadedSource1 && loadedSource2)
            postMessage("PASS: Both scripts were executed");
        else
            postMessage("FAIL: scripts did not execute correctly");

        resetLoadFlags();

        postMessage("Testing multiple arguments, with second argument throwing in toString():");
        try {
            importScripts(source1, {toString: function() { throw new Error("user error"); }}, source2);
        } catch (e) {
            if (e.message.includes("user error"))
                postMessage("PASS: User error recieved in toString");
            else
                postMessage("FAIL: Unexpected error: " + e);
        }
        if (loadedSource1 || loadedSource2)
            postMessage("FAIL: scripts should not have been executed");
        else
            postMessage("PASS: No script was executed");

        resetLoadFlags();

    } catch (e) {
        postMessage("FAIL: Unexpected exception: " + e);
    } finally {
        postMessage("DONE");
    }
}

