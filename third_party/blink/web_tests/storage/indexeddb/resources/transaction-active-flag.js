if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB transaction internal active flag.");

indexedDBTest(prepareDatabase, runTransaction);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    evalAndLog("store = db.createObjectStore('store')");
    evalAndLog("store.createIndex('index', 'keypath')");
}

function runTransaction()
{
    debug("");
    debug("runTransaction():");
    evalAndLog("transaction = db.transaction('store', 'readwrite', {durability: 'relaxed'})");

    debug("");
    debug("Verify that transactions are created with |active| flag set:");
    evalAndLog("store = transaction.objectStore('store')");
    evalAndLog("index = store.index('index')");
    for (i = 0; i < statements.length; ++i) {
        shouldNotThrow(statements[i]);
    }

    debug("");
    debug("Transaction shouldn't be active inside a non-IDB-event callback");
    evalAndLog("setTimeout(testTimeout, 0)");

    timeoutComplete = false;

    // Keep transaction alive until timeout callback completes - don't log
    // anything here as this could run a variable number of times depending
    // on the port.
    function busy() {
        if (timeoutComplete) {
            testEventCallback();
            return;
        }
        busyRequest = transaction.objectStore('store').get(0);
        busyRequest.onerror = unexpectedErrorCallback;
        busyRequest.onsuccess = function () {
            busy();
        };
    }
    busy();

    transaction.oncomplete = transactionComplete;
}

var statements = [
    "store.add(0, 0)",
    "store.put(0, 0)",
    "store.get(0)",
    "store.get(IDBKeyRange.only(0))",
    "store.delete(0)",
    "store.delete(IDBKeyRange.only(0))",
    "store.count()",
    "store.count(0)",
    "store.count(IDBKeyRange.only(0))",
    "store.clear()",
    "store.openCursor()",
    "store.openCursor(0)",
    "store.openCursor(0, 'next')",
    "store.openCursor(IDBKeyRange.only(0))",
    "store.openCursor(IDBKeyRange.only(0), 'next')",
    "index.get(0)",
    "index.get(IDBKeyRange.only(0))",
    "index.getKey(0)",
    "index.getKey(IDBKeyRange.only(0))",
    "index.count()",
    "index.count(0)",
    "index.count(IDBKeyRange.only(0))",
    "index.openCursor()",
    "index.openCursor(0)",
    "index.openCursor(0, 'next')",
    "index.openCursor(IDBKeyRange.only(0))",
    "index.openCursor(IDBKeyRange.only(0), 'next')",
    "index.openKeyCursor()",
    "index.openKeyCursor(0)",
    "index.openKeyCursor(0, 'next')",
    "index.openKeyCursor(IDBKeyRange.only(0))",
    "index.openKeyCursor(IDBKeyRange.only(0), 'next')"
];

function testTimeout()
{
    debug("");
    debug("testTimeout():");
    evalAndLog("store = transaction.objectStore('store')");
    evalAndLog("index = store.index('index')");
    for (i = 0; i < statements.length; ++i) {
        evalAndExpectException(statements[i], "0", "'TransactionInactiveError'");
    }
    timeoutComplete = true;
}

function testEventCallback()
{
    debug("");
    debug("testEventCallback():");

    debug("Transaction should be active inside a non-IDB-event callback");
    evalAndLog("store = transaction.objectStore('store')");
    evalAndLog("index = store.index('index')");
    for (i = 0; i < statements.length; ++i) {
        shouldNotThrow(statements[i]);
    }
}

function transactionComplete()
{
    debug("");
    debug("transactionComplete():");
    evalAndExpectException("store = transaction.objectStore('store')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    finishJSTest();
}
