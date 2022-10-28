if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Ensure IDBDatabase wrapper isn't prematurely collected.");

test();

function test()
{
    setDBNameFromPath();
    var deleteRequest = evalAndLog("indexedDB.deleteDatabase(dbname)");
    deleteRequest.onblocked = unexpectedBlockedCallback;
    deleteRequest.onerror = unexpectedErrorCallback;
    deleteRequest.onsuccess = openDB;
}

function openDB()
{
    preamble();

    var openRequest = evalAndLog("indexedDB.open(dbname, 1)");
    openRequest.onblocked = unexpectedBlockedCallback;
    openRequest.onerror = unexpectedErrorCallback;
    openRequest.onupgradeneeded = onUpgradeNeeded;
    openRequest.onsuccess = openSuccess;
}

function onUpgradeNeeded(evt) {
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("db.createObjectStore('store').createIndex('index', 'keyPath')");
    evalAndLog("db = null");
}

function openSuccess(evt)
{
    preamble(evt);
    var db = event.target.result;
    db.onversionchange = onVersionChange;
    evalAndLog("sawVersionChangeEvent = false");

    // All these local references should get collected, but the database's
    // wrapper shouldn't get collected before the database itself.
    var transaction = db.transaction('store', 'readonly', {durability: 'relaxed'});
    var objectStore = transaction.objectStore('store');
    var request = objectStore.get(0);
    request.onsuccess = function() {
        setTimeout(collectGarbage, 0);
    };
}

function onVersionChange(evt)
{
    preamble(evt);
    evalAndLog("event.target.close()");
    evalAndLog("sawVersionChangeEvent = true");
}

function collectGarbage()
{
    preamble();
    evalAndLog("self.gc()");

    setTimeout(openAgain, 0);
}

function openAgain()
{
    preamble();

    var openRequest = evalAndLog("indexedDB.open(dbname, 2)");
    openRequest.onblocked = unexpectedBlockedCallback;
    openRequest.onerror = unexpectedErrorCallback;
    openRequest.onsuccess = openAgainSuccess;
}

function openAgainSuccess(evt)
{
    preamble(evt);
    shouldBeTrue("sawVersionChangeEvent");
    finishJSTest();
}
