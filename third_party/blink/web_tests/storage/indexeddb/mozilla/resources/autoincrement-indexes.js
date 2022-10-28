// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_autoIncrement_indexes.html?force=1
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB indexes against autoincrementing keys");

indexedDBTest(prepareDatabase, setVersionComplete);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

    objectStore = evalAndLog("objectStore = db.createObjectStore('autoincrement-id', { keyPath: 'id', autoIncrement: true });");
    evalAndLog("objectStore.createIndex('first', 'first');");
    evalAndLog("objectStore.createIndex('second', 'second');");
    evalAndLog("objectStore.createIndex('third', 'third');");
    data = evalAndLog("data = { first: 'foo', second: 'foo', third: 'foo' };");
    request = evalAndLog("request = objectStore.add(data);");
    request.onsuccess = setupIndexes;
    request.onerror = unexpectedErrorCallback;
}

function setupIndexes()
{
    key = evalAndLog("key = event.target.result;");
    shouldBeFalse("key == null");
    debug("expected key is " + key);
}

function setVersionComplete()
{
    objectStore = evalAndLog("objectStore = db.transaction('autoincrement-id', 'readonly', {durability: 'relaxed'}).objectStore('autoincrement-id');");
    first = evalAndLog("first = objectStore.index('first');");
    request = evalAndLog("request = first.get('foo');");
    request.onsuccess = checkFirstIndexAndPrepareSecond;
    request.onerror = unexpectedErrorCallback;
}

function checkFirstIndexAndPrepareSecond()
{
    shouldBe("event.target.result.id", "key");
    second = evalAndLog("second = objectStore.index('second');");
    request = evalAndLog("request = second.get('foo');");
    request.onsuccess = checkSecondIndexAndPrepareThird;
    request.onerror = unexpectedErrorCallback;
}

function checkSecondIndexAndPrepareThird()
{
    shouldBe("event.target.result.id", "key");
    third = evalAndLog("third = objectStore.index('third');");
    request = evalAndLog("request = third.get('foo');");
    request.onsuccess = checkThirdIndex;
    request.onerror = unexpectedErrorCallback;
}

function checkThirdIndex()
{
    shouldBe("event.target.result.id", "key");
    finishJSTest();
}
