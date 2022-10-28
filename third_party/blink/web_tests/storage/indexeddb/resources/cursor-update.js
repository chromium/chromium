if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB's cursor update.");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    debug("setVersionSuccess():");
    self.trans = evalAndLog("trans = event.target.transaction");
    shouldBeNonNull("trans");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = openBasicCursor;

    deleteAllObjectStores(db);

    var objectStore = evalAndLog("objectStore = db.createObjectStore('basicStore')");
    evalAndLog("objectStore.add('myValue1', 'myKey1').onerror = unexpectedErrorCallback");
    evalAndLog("objectStore.add('myValue2', 'myKey2').onerror = unexpectedErrorCallback");
    evalAndLog("objectStore.add('myValue3', 'myKey3').onerror = unexpectedErrorCallback");
    evalAndLog("objectStore.add('myValue4', 'myKey4').onerror = unexpectedErrorCallback");

    objectStore = evalAndLog("objectStore = db.createObjectStore('autoIncrementStore', {autoIncrement: true})");
    evalAndLog("objectStore.add('foo1').onerror = unexpectedErrorCallback");
    evalAndLog("objectStore.add('foo2').onerror = unexpectedErrorCallback");
    evalAndLog("objectStore.add('foo3').onerror = unexpectedErrorCallback");
    evalAndLog("objectStore.add('foo4').onerror = unexpectedErrorCallback");

    objectStore = evalAndLog("objectStore = db.createObjectStore('keyPathStore', {keyPath: 'id'})");
    evalAndLog("objectStore.createIndex('numberIndex', 'number')");
    evalAndLog("objectStore.add({number: 1, id: 1}).onerror = unexpectedErrorCallback");
    evalAndLog("objectStore.add({number: 2, id: 2}).onerror = unexpectedErrorCallback");
    evalAndLog("objectStore.add({number: 3, id: 3}).onerror = unexpectedErrorCallback");
    evalAndLog("objectStore.add({number: 4, id: 4}).onerror = unexpectedErrorCallback");
}

function openBasicCursor()
{
    debug("openBasicCursor()");
    evalAndLog("trans = db.transaction(['basicStore', 'autoIncrementStore', 'keyPathStore'], 'readwrite')");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = testReadOnly;

    keyRange = IDBKeyRange.lowerBound("myKey1");
    self.objectStore = evalAndLog("trans.objectStore('basicStore')");
    request = evalAndLog("objectStore.openCursor(keyRange)");
    request.onsuccess = basicUpdateCursor;
    request.onerror = unexpectedErrorCallback;
    counter = 1;
}

function basicUpdateCursor()
{
    debug("basicUpdateCursor()");
    shouldBe("event.target.source", "objectStore");
    if (event.target.result == null) {
        shouldBe("counter", "5");
        counter = 1;

        request = evalAndLog("trans.objectStore('basicStore').openCursor(keyRange)");
        request.onsuccess = basicCheckCursor;
        request.onerror = unexpectedErrorCallback;
        return;
    }

    request = evalAndLog("event.target.result.update('myUpdatedValue' + counter++)");
    request.onsuccess = function() { evalAndLog("event.target.source.continue()"); };
    request.onerror = unexpectedErrorCallback;
}

function basicCheckCursor()
{
    debug("basicCheckCursor()");
    if (event.target.result == null) {
        shouldBe("counter", "5");
        counter = 1;

        keyRange = IDBKeyRange.lowerBound(1);
        request = evalAndLog("trans.objectStore('autoIncrementStore').openCursor(keyRange)");
        request.onsuccess = autoIncrementUpdateCursor;
        request.onerror = unexpectedErrorCallback;
        return;
    }

    shouldBeEqualToString("event.target.result.key", "myKey" + counter);
    shouldBeEqualToString("event.target.result.value", "myUpdatedValue" + counter++);
    evalAndLog("event.target.result.continue()");
}

function autoIncrementUpdateCursor()
{
    debug("autoIncrementUpdateCursor()");
    if (event.target.result == null) {
        shouldBe("counter", "5");
        counter = 1;

        request = evalAndLog("trans.objectStore('autoIncrementStore').openCursor(keyRange)");
        request.onsuccess = autoIncrementCheckCursor;
        request.onerror = unexpectedErrorCallback;
        return;
    }

    request = evalAndLog("event.target.result.update('myUpdatedFoo' + counter++)");
    request.onsuccess = function() { evalAndLog("event.target.source.continue()"); };
    request.onerror = unexpectedErrorCallback;
}

function autoIncrementCheckCursor()
{
    debug("autoIncrementCheckCursor()");
    if (event.target.result == null) {
        shouldBe("counter", "5");
        counter = 1;

        keyRange = IDBKeyRange.lowerBound(1);
        request = evalAndLog("trans.objectStore('keyPathStore').openCursor(keyRange)");
        request.onsuccess = keyPathUpdateCursor;
        request.onerror = unexpectedErrorCallback;
        return;
    }

    shouldBe("event.target.result.key", "counter");
    shouldBeEqualToString("event.target.result.value", "myUpdatedFoo" + counter++);
    evalAndLog("event.target.result.continue()");
}

function keyPathUpdateCursor()
{
    debug("keyPathUpdateCursor()");
    if (event.target.result == null) {
        shouldBe("counter", "5");
        counter = 1;

        request = evalAndLog("trans.objectStore('keyPathStore').openCursor(keyRange)");
        request.onsuccess = keyPathCheckCursor;
        request.onerror = unexpectedErrorCallback;
        return;
    }

    evalAndExpectException("event.target.result.update({id: 100 + counter, number: 100 + counter})", "0", "'DataError'");

    request = evalAndLog("event.target.result.update({id: counter, number: 100 + counter++})");
    request.onsuccess = function() { evalAndLog("event.target.source.continue()"); };
    request.onerror = unexpectedErrorCallback;
}

function keyPathCheckCursor()
{
    debug("keyPathCheckCursor()");
    if (event.target.result == null) {
        shouldBe("counter", "5");
        counter = 1;

        keyRange = IDBKeyRange.lowerBound(101);
        request = evalAndLog("trans.objectStore('keyPathStore').index('numberIndex').openKeyCursor(keyRange)");
        request.onsuccess = keyCursor;
        request.onerror = unexpectedErrorCallback;
        return;
    }

    shouldBe("event.target.result.key", "counter");
    shouldBe("event.target.result.value.id", "counter");
    shouldBe("event.target.result.value.number", (counter + 100).toString());
    counter++;
    evalAndLog("event.target.result.continue()");
}

function keyCursor()
{
    debug("keyCursor()");
    if (event.target.result == null) {
        shouldBe("counter", "5");
        return;
    }

    shouldBe("event.target.result.key", "counter + 100");
    shouldBe("event.target.result.primaryKey", "counter");

    evalAndExpectException("event.target.result.update({id: counter, number: counter + 200})",
                           "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    counter++;
    evalAndLog("event.target.result.continue();");
}

function testReadOnly()
{
    debug("openBasicCursor()");
    evalAndLog("trans = db.transaction('basicStore', 'readonly', {durability: 'relaxed'})");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = transactionComplete;

    keyRange = IDBKeyRange.lowerBound("myKey1");
    self.objectStore = evalAndLog("trans.objectStore('basicStore')");
    request = evalAndLog("objectStore.openCursor(keyRange)");
    request.onsuccess = attemptUpdate;
    request.onerror = unexpectedErrorCallback;
}

function attemptUpdate()
{
    debug("attemptUpdate()");
    self.cursor = event.target.result;
    if (self.cursor) {
        evalAndExpectException("cursor.update('myUpdatedValue')", "0", "'ReadOnlyError'");
        evalAndLog("cursor.continue()");
    }
}

function transactionComplete()
{
    debug("transactionComplete()");
    finishJSTest();
}
