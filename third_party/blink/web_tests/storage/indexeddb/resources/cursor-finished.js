description("Ensure cursor calls behave as expected after cursor has run to the end.");

function test()
{
    setDBNameFromPath();
    prepareDatabase();
}

function prepareDatabase()
{
    preamble();
    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = onDeleteSuccess;
}

function onDeleteSuccess(evt)
{
    preamble(evt);
    request = evalAndLog("indexedDB.open(dbname, 1)");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = onUpgradeNeeded;
    request.onsuccess = onOpenSuccess;
}

function onUpgradeNeeded(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");
    evalAndLog("store.put(1, 1)");
    evalAndLog("store.put(2, 2)");
}

function onOpenSuccess(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("transaction = db.transaction('store', 'readwrite', {durability: 'relaxed'})");
    evalAndLog("store = transaction.objectStore('store')");
    evalAndLog("count = 0");
    evalAndLog("cursorRequest = store.openCursor()");
    cursorRequest.onerror = unexpectedErrorCallback;
    cursorRequest.onsuccess = onCursorSuccess;
}

function onCursorSuccess(evt)
{
    preamble(evt);
    evalAndLog("cursor = event.target.result");
    if (count < 2) {
        shouldBeNonNull("cursor");
        evalAndLog("count++");
        evalAndLog("savedCursor = cursor");
        evalAndLog("cursor.continue()");
    } else {
        shouldBeNull("cursor");
        shouldBeNonNull("savedCursor");

        debug("");
        evalAndExpectException("savedCursor.update('value')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
        evalAndExpectException("savedCursor.advance(1)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
        evalAndExpectException("savedCursor.continue()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
        evalAndExpectException("savedCursor.continue('key')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
        evalAndExpectException("savedCursor.delete()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

        debug("");
        finishJSTest();
    }
}

test();
