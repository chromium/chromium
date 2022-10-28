if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Call db.close() in the complete handler for a version change transaction, before the success event associated with the open call fires");

function test()
{
    setDBNameFromPath();

    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onsuccess = deleteSuccess;
    request.onerror = unexpectedErrorCallback;
}

function deleteSuccess(evt) {
    evalAndLog("request = indexedDB.open(dbname, 7)");
    request.onupgradeneeded = upgradeNeeded;
    request.onerror = openError;
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = unexpectedSuccessCallback;
}

var sawTransactionComplete = false;
function upgradeNeeded(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    shouldBe("event.newVersion", "7");
    evalAndLog("db.createObjectStore('os')");

    evalAndLog("transaction = event.target.transaction");
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = function(e)
    {
        debug("");
        debug("transaction.oncomplete:");
        evalAndLog("sawTransactionComplete = true");
        evalAndLog("db.close()");
    };
}

function openError(evt)
{
    preamble(evt);
    shouldBeTrue("sawTransactionComplete");
    shouldBeUndefined("event.target.result");
    shouldBeNonNull("event.target.error");
    shouldBeEqualToString("event.target.error.name", "AbortError");
    evalAndExpectException("transaction = db.transaction('os', 'readwrite', {durability: 'relaxed'})", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    finishJSTest();
}

test();
