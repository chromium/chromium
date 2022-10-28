if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Checks that garbage collection doesn't reclaim objects with pending activity");

indexedDBTest(prepareDatabase, testTransaction);
function prepareDatabase(evt)
{
    preamble(evt);
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    evalAndLog("store = db.createObjectStore('store')");
    evalAndLog("store.put(0, 0)");
}

function testTransaction()
{
    preamble();
    evalAndLog("transaction = db.transaction('store', 'readonly', {durability: 'relaxed'})");
    evalAndLog("transaction.oncomplete = transactionOnComplete");
    evalAndLog("transaction = null");
    evalAndLog("self.gc()");
}

function transactionOnComplete()
{
    testPassed("Transaction 'complete' event fired.");
    testRequest();
}

function testRequest()
{
    preamble();
    evalAndLog("transaction = db.transaction('store', 'readonly', {durability: 'relaxed'})");
    evalAndLog("store = transaction.objectStore('store')");
    evalAndLog("request = store.get(0)");
    evalAndLog("request.onsuccess = requestOnSuccess");
    evalAndLog("request = null");
    evalAndLog("self.gc()");
}

function requestOnSuccess()
{
    testPassed("Request 'success' event fired.");
    testCursorRequest();
}

function testCursorRequest()
{
    preamble();
    evalAndLog("transaction = db.transaction('store', 'readonly', {durability: 'relaxed'})");
    evalAndLog("store = transaction.objectStore('store')");
    evalAndLog("request = store.openCursor()");
    evalAndLog("request.onsuccess = cursorRequestOnFirstSuccess");
}

function cursorRequestOnFirstSuccess()
{
    testPassed("Request 'success' event fired.");
    evalAndLog("cursor = request.result");
    evalAndLog("cursor.continue()");
    evalAndLog("request.onsuccess = cursorRequestOnSecondSuccess");
    evalAndLog("request = null");
    evalAndLog("self.gc()");
}

function cursorRequestOnSecondSuccess()
{
    testPassed("Request 'success' event fired.");

    debug("");
    finishJSTest();
}
