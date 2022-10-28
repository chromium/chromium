// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_odd_result_order.html?raw=1
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB: request result persists in setTimeout callback");

indexedDBTest(prepareDatabase, openSuccess);
function prepareDatabase()
{
    request = event.target;
    db = event.target.result;
    objectStore = evalAndLog("objectStore = db.createObjectStore('foo', { keyPath: 'key', autoIncrement: true });");
    index = evalAndLog("index = objectStore.createIndex('foo', 'index');");
}

function openSuccess()
{
    debug("openSuccess():");
    db = evalAndLog("db = null;");
    setTimeout(function() {
        db = evalAndLog("db = request.result;");
        checkDatabaseType();
        addRecord();
    }, 0);
}

function checkDatabaseType()
{
    debug("checkDatabaseType():");
    shouldBeTrue("db instanceof IDBDatabase");
    db.onerror = unexpectedErrorCallback;
}

function addRecord()
{
    debug("addRecord():");
    objectStore = evalAndLog("objectStore = db.transaction('foo', 'readwrite', {durability: 'relaxed'}).objectStore('foo');");
    data = evalAndLog("data = { key: 5, index: 10 };");
    request = evalAndLog("request = objectStore.add(data);");
    request.onsuccess = addSuccess;
    request.onerror = unexpectedErrorCallback;
}

function addSuccess()
{
    debug("addSuccess():");
    key = evalAndLog("key = null;");
    setTimeout(function() {
      key = evalAndLog("key = request.result;");
      checkAddedKey();
    }, 0);
}

function checkAddedKey()
{
    debug("checkAddedKey():");
    shouldBe("key", "data.key");
    objectStore = evalAndLog("objectStore = db.transaction('foo', 'readonly', {durability: 'relaxed'}).objectStore('foo');");
    request = evalAndLog("request = objectStore.get(data.key);");
    request.onsuccess = getSuccess;
    request.onerror = unexpectedErrorCallback;
}

function getSuccess()
{
    debug("getSuccess():");
    record = evalAndLog("record = null;");
    setTimeout(function() {
      record = evalAndLog("record = request.result;");
      checkRetrievedKey();
    }, 0);
}

function checkRetrievedKey()
{
    debug("checkRetrievedKey():");
    shouldBe("record.key", "data.key");
    shouldBe("record.index", "data.index");
    deleteRecord();
}

function deleteRecord()
{
    debug("deleteRecord():");
    objectStore = evalAndLog("objectStore = db.transaction('foo', 'readwrite', {durability: 'relaxed'}).objectStore('foo');");
    request = evalAndLog("request = objectStore.delete(data.key);");
    request.onsuccess = deleteSuccess;
    request.onerror = unexpectedErrorCallback;
}

function deleteSuccess()
{
    debug("deleteSuccess():");
    finishJSTest();
}
