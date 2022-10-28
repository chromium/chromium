if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test read-only transactions in IndexedDB.");

indexedDBTest(prepareDatabase, setVersionDone);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    store = evalAndLog("store = db.createObjectStore('store')");
    evalAndLog("store.put('x', 'y')");
}

function setVersionDone()
{
    trans = evalAndLog("trans = db.transaction('store', 'readonly', {durability: 'relaxed'})");
    evalAndExpectException("trans.objectStore('store').put('a', 'b')", "0", "'ReadOnlyError'");

    trans = evalAndLog("trans = db.transaction('store', 'readonly', {durability: 'relaxed'})");
    evalAndExpectException("trans.objectStore('store').delete('x')", "0", "'ReadOnlyError'");

    trans = evalAndLog("trans = db.transaction('store', 'readonly', {durability: 'relaxed'})");
    cur = evalAndLog("cur = trans.objectStore('store').openCursor()");
    cur.onsuccess = gotCursor;
    cur.onerror = unexpectedErrorCallback;
}

function gotCursor()
{
    shouldBeFalse("!event.target.result");
    evalAndExpectException("event.target.result.delete()", "0", "'ReadOnlyError'");

    finishJSTest();
}
