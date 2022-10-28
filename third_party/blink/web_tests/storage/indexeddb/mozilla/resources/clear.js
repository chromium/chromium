// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_clear.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's clearing an object store");

indexedDBTest(prepareDatabase, clear);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    objectStore = evalAndLog("objectStore = db.createObjectStore('foo', { autoIncrement: true });");
    request = evalAndLog("request = objectStore.add({});");
    request.onerror = unexpectedErrorCallback;
}

function clear()
{
    evalAndExpectException("db.transaction('foo', 'readonly', {durability: 'relaxed'}).objectStore('foo').clear();", "0", "'ReadOnlyError'");
    transaction = evalAndLog("db.transaction('foo', 'readwrite', {durability: 'relaxed'})");
    evalAndLog("transaction.objectStore('foo').clear();");
    transaction.oncomplete = cleared;
    transaction.onabort = unexpectedAbortCallback;
}

function cleared()
{
    request = evalAndLog("request = db.transaction('foo', 'readonly', {durability: 'relaxed'}).objectStore('foo').openCursor();");
    request.onsuccess = areWeClearYet;
    request.onerror = unexpectedErrorCallback;
}

function areWeClearYet()
{
    cursor = evalAndLog("cursor = request.result;");
    shouldBe("cursor", "null");
    finishJSTest();
}
