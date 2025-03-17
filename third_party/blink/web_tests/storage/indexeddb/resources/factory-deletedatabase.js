if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB's indexedDB.deleteDatabase().");

indexedDBTest(prepareDatabase, getValue);
function prepareDatabase()
{
    db = event.target.result;

    store = evalAndLog("store = db.createObjectStore('storeName', null)");

    self.index = evalAndLog("store.createIndex('indexName', '')");
    shouldBeTrue("store.indexNames.contains('indexName')");

    request = evalAndLog("store.add('value', 'key')");
    request.onerror = unexpectedErrorCallback;
}

function getValue()
{
    transaction = evalAndLog("db.transaction('storeName', 'readwrite')");
    transaction.onabort = unexpectedErrorCallback;
    var store = evalAndLog("store = transaction.objectStore('storeName')");

    request = evalAndLog("store.get('key')");
    request.onsuccess = addIndex;
    request.onerror = unexpectedErrorCallback;
}

function addIndex()
{
    shouldBeEqualToString("event.target.result", "value");
    evalAndLog("db.close()");

    request = evalAndLog("indexedDB.open(dbname, 2)");
    request.onsuccess = deleteDatabase;
    request.onerror = unexpectedErrorCallback;
}

function deleteDatabase()
{
    evalAndLog("db = event.target.result");
    db.onversionchange = function() { evalAndLog("db.close()"); }
    request = evalAndLog("request = indexedDB.deleteDatabase(dbname)");
    request.onsuccess = reopenDatabase;
    request.onerror = unexpectedErrorCallback;
}

function reopenDatabase()
{
    shouldBeUndefined("request.result");
    request = evalAndLog("indexedDB.open(dbname, 3)");
    request.onupgradeneeded = verifyNotFound;
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
}

function verifyNotFound()
{
    db = evalAndLog("db = event.target.result");
    shouldBe("db.objectStoreNames.length", "0");

    finishJSTest();
}
