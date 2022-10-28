if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that continue() calls against cursors are validated by direction.");

indexedDBTest(prepareDatabase, testCursors);
function prepareDatabase()
{
    db = event.target.result;
    evalAndLog("store = db.createObjectStore('store')");
    for (i = 1; i <= 10; ++i) {
        evalAndLog("store.put(" + i + "," + i + ")");
    }
}

function testCursors()
{
    evalAndLog("trans = db.transaction('store', 'readonly', {durability: 'relaxed'})");
    evalAndLog("store = trans.objectStore('store')");
    testForwardCursor();
}

function testForwardCursor()
{
    evalAndLog("request = store.openCursor(IDBKeyRange.bound(-Infinity, Infinity), 'next')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        evalAndLog("cursor = request.result");
        shouldBeNonNull("cursor");
        debug("Expect DataError if: The parameter is less than or equal to this cursor's position and this cursor's direction is \"next\" or \"nextunique\".");
        shouldBe("cursor.key", "1");
        evalAndExpectException("cursor.continue(-1)", "0", "'DataError'");

        testReverseCursor();
    };
}

function testReverseCursor()
{
    evalAndLog("request = store.openCursor(IDBKeyRange.bound(-Infinity, Infinity), 'prev')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        evalAndLog("cursor = request.result");
        shouldBeNonNull("cursor");
        debug("Expect DataError if: The parameter is greater than or equal to this cursor's position and this cursor's direction is \"prev\" or \"prevunique\".");
        shouldBe("cursor.key", "10");
        evalAndExpectException("cursor.continue(11)", "0", "'DataError'");

        finishJSTest();
    };
}
