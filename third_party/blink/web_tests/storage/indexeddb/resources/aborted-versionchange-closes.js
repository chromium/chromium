if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that an aborted 'versionchange' transaction closes the connection.");

indexedDBTest(prepareDatabase, onOpen, {version: 1});
function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("db.createObjectStore('store')");
}

function onOpen(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("db.close()");
    openAgain();
}

function openAgain() {
    preamble();
    evalAndLog("request = indexedDB.open(dbname, 2)");
    request.onsuccess = unexpectedSuccessCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = onUpgradeNeeded;
    request.onerror = onOpenError;
}

function onUpgradeNeeded(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("transaction = event.target.transaction");
    evalAndLog("sawTransactionAbort = false");
    transaction.oncomplete = unexpectedCompleteCallback;
    transaction.onabort = onTransactionAbort;
    transaction.abort();
}

function onTransactionAbort(evt)
{
    preamble(evt);
    evalAndLog("sawTransactionAbort = true");
    debug("creating a transaction should fail because connection is closed:");
    evalAndExpectException("db.transaction('store', 'readonly', {durability: 'relaxed'})", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
}

function onOpenError(evt)
{
    preamble(evt);
    shouldBeTrue("sawTransactionAbort");
    debug("creating a transaction should fail because connection is closed:");
    evalAndExpectException("db.transaction('store', 'readonly', {durability: 'relaxed'})", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    finishJSTest();
}
