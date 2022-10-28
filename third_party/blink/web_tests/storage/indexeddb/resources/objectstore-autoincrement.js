if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB's IDBObjectStore auto-increment feature.");

indexedDBTest(prepareDatabase, setVersionCompleted);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

    debug("createObjectStore():");
    self.store = evalAndLog("store = db.createObjectStore('StoreWithKeyPath', {keyPath: 'id', autoIncrement: true})");
    evalAndLog("db.createObjectStore('StoreWithAutoIncrement', {autoIncrement: true})");
    evalAndLog("db.createObjectStore('PlainOldStore', {autoIncrement: false})");
    evalAndLog("db.createObjectStore('StoreWithLongKeyPath', {keyPath: 'a.b.c.id', autoIncrement: true})");
    var storeNames = evalAndLog("storeNames = db.objectStoreNames");

    shouldBeEqualToString("store.name", "StoreWithKeyPath");
    shouldBeEqualToString("store.keyPath", "id");
    shouldBe("storeNames.contains('StoreWithKeyPath')", "true");
    shouldBe("storeNames.contains('StoreWithAutoIncrement')", "true");
    shouldBe("storeNames.contains('PlainOldStore')", "true");
    shouldBe("storeNames.length", "4");
}

function setVersionCompleted()
{
    debug("setVersionCompleted():");

    self.trans = evalAndLog("trans = db.transaction(['StoreWithKeyPath', 'StoreWithAutoIncrement', 'PlainOldStore'], 'readwrite')");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = testLongKeyPath;

    self.store = evalAndLog("store = trans.objectStore('StoreWithKeyPath')");

    debug("Insert into object store with auto increment and key path, with key in the object.");
    request = evalAndLog("store.add({name: 'Jeffersson', number: '7010', id: 3})");
    request.onsuccess = addJefferssonSuccess;
    request.onerror = unexpectedErrorCallback;
}

function addJefferssonSuccess()
{
    debug("addJefferssonSuccess():");
    shouldBe("event.target.result", "3");

    debug("Insert into object store with auto increment and key path, without key in the object.");
    request = evalAndLog("store.add({name: 'Lincoln', number: '7012'})");
    request.onsuccess = addLincolnWithInjectKeySuccess;
    request.onerror = unexpectedErrorCallback;
}

function addLincolnWithInjectKeySuccess()
{
    debug("addLincolnWithInjectKeySuccess():");
    shouldBe("event.target.result", "4");

    result = evalAndLog("store.get(4)");
    result.onsuccess = getLincolnAfterInjectedKeySuccess;
    result.onerror = unexpectedErrorCallback;
}

function getLincolnAfterInjectedKeySuccess()
{
    debug("getLincolnAfterInjectedKeySuccess():");
    shouldBeEqualToString("event.target.result.name", "Lincoln");
    shouldBeEqualToString("event.target.result.number", "7012");
    shouldBe("event.target.result.id", "4");

    self.store = evalAndLog("store = trans.objectStore('StoreWithAutoIncrement')");
    debug("Insert into object store with key gen using explicit key");
    request = evalAndLog("store.add({name: 'Lincoln', number: '7012'}, 5)");
    request.onsuccess = addLincolnWithExplicitKeySuccess;
    request.onerror = unexpectedErrorCallback;
}

function addLincolnWithExplicitKeySuccess()
{
    debug("addLincolnWithExplicitKeySuccess():");
    shouldBe("event.target.result", "5");

    request = evalAndLog("store.get(5)");
    request.onsuccess = getLincolnSuccess;
    request.onerror = unexpectedErrorCallback;
}

function getLincolnSuccess()
{
    debug("getLincolnSuccess():");
    shouldBeEqualToString("event.target.result.name", "Lincoln");
    shouldBeEqualToString("event.target.result.number", "7012");

    request = evalAndLog("store.put({name: 'Abraham', number: '2107'})");
    request.onsuccess = putAbrahamSuccess;
    request.onerror = unexpectedErrorCallback;
}

function putAbrahamSuccess()
{
    debug("putAbrahamSuccess():");
    shouldBe("event.target.result", "6");

    request = evalAndLog("store.get(6)");
    request.onsuccess = getAbrahamSuccess;
    request.onerror = unexpectedErrorCallback;
}

function getAbrahamSuccess()
{
    debug("getAbrahamSuccess():");
    shouldBeEqualToString("event.target.result.name", "Abraham");
    shouldBeEqualToString("event.target.result.number", "2107");

    self.store = evalAndLog("store = trans.objectStore('PlainOldStore')");
    debug("Try adding with no key to object store without auto increment.");
    evalAndExpectException("store.add({name: 'Adam'})", "0", "'DataError'");
    request = evalAndLog("store.add({name: 'Adam'}, 1)");
    request.onsuccess = addAdamSuccess;
    request.onerror = unexpectedErrorCallback;
}

function addAdamSuccess()
{
    debug("addAdamSuccess():");
    shouldBe("event.target.result", "1");
}

function testLongKeyPath()
{
    debug("testLongKeyPath():");
    trans = evalAndLog("trans = db.transaction('StoreWithLongKeyPath', 'readwrite', {durability: 'relaxed'})");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = finishJSTest;

    store = evalAndLog("store = trans.objectStore('StoreWithLongKeyPath')");
    request = evalAndLog("store.add({foo: 'bar'})");
    request.onerror = unexpectedErrorCallback;
    request = evalAndLog("store.add({foo: 'bar', a: {}})");
    request.onerror = unexpectedErrorCallback;
    request = evalAndLog("store.add({foo: 'bar', a: {b: {}}})");
    request.onerror = unexpectedErrorCallback;
    request = evalAndLog("store.add({foo: 'bar', a: {b: {c: {}}}})");
    request.onerror = unexpectedErrorCallback;
    cursorRequest = evalAndLog("store.openCursor()");
    cursorRequest.onerror = unexpectedErrorCallback;
    evalAndLog("expected = null");
    evalAndLog("count = 0");
    cursorRequest.onsuccess = function () {
        cursor = cursorRequest.result;
        if (!cursor) {
            shouldBe("count", "4");
            return;
        }
        if (expected === null) {
            evalAndLog("expected = cursor.value.a.b.c.id + 1");
        } else {
            shouldBeEqualToString("cursor.value.foo", "bar");
            shouldBe("cursor.value.a.b.c.id", "expected");
            evalAndLog("expected = cursor.value.a.b.c.id + 1");
        }
        count++;
        cursor.continue();
    };
}
