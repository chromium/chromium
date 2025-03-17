if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB's cursor value property.");

indexedDBTest(prepareDatabase, testCursor);
function prepareDatabase()
{
    db = event.target.result;
    evalAndLog("db.createObjectStore('store')");
}

function testCursor()
{
    debug("");
    debug("testCursor():");
    evalAndLog("transaction = db.transaction('store', 'readwrite')");
    evalAndLog("store = transaction.objectStore('store')");
    evalAndLog("store.put({a: 1, b: 10}, 'key1')");
    evalAndLog("store.put({a: 2, b: 20}, 'key2')");
    evalAndLog("store.put({a: 3, b: 30}, 'key3')");
    evalAndLog("store.put({a: 4, b: 40}, 'key4')");
    evalAndLog("store.put({a: 5, b: 50}, 'key5')");
    evalAndLog("request = store.openCursor()");
    request.onerror = unexpectedErrorCallback;
    var index = 0;
    request.onsuccess = function() {
        debug("");
        debug("----------");
        debug("Value at index: " + index);
        evalAndLog("cursor = request.result");
        if (index == 0) {
            ensureObjectData(1, 10, 'key1');
            cursor.continue();
            index++;
        } else if (index == 1) {
            ensureObjectData(2, 20, 'key2');
            cursor.advance(2);
            index += 2;
        } else if (index == 3) {
            ensureObjectData(4, 40, 'key4');
        } else {
            testFailed("Bad index: " + index);
        }
    };

    transaction.oncomplete = ensureModificationsNotPersisted;
}

function ensureObjectData(a, b, key)
{
    expectedA = a;
    expectedB = b;
    expectedKey = key;
    shouldBe("cursor.key", "expectedKey");

    debug("");
    debug("Check expected values:");
    shouldBe("cursor.value.a", "expectedA");
    shouldBe("cursor.value.b", "expectedB");
    shouldBe("cursor.value.foo", "undefined");

    debug("");
    debug("Modify values:");
    evalAndLog("cursor.value.a = 3");
    evalAndLog("delete cursor.value.b");
    evalAndLog("cursor.value.foo = 'bar'");

    debug("");
    debug("Ensure modifications are retained:");
    shouldBe("cursor.value.a", "3");
    shouldBe("cursor.value.b", "undefined");
    shouldBe("cursor.value.foo", "'bar'");

    // make sure to test GC before holding a specific ref to the value
    debug("");
    debug("Check object value survives gc");
    evalAndLog("gc()");
    shouldBe("cursor.value.a", "3");
    shouldBe("cursor.value.b", "undefined");
    shouldBe("cursor.value.foo", "'bar'");

    debug("");
    debug("Check object identity");
    evalAndLog("localValueRef = cursor.value");
    shouldBe("localValueRef", "cursor.value");
}

function ensureModificationsNotPersisted()
{
    debug("");
    debug("ensureModificationsNotPersisted():");
    evalAndLog("transaction = db.transaction('store', 'readonly')");
    evalAndLog("store = transaction.objectStore('store')");
    evalAndLog("request = store.openCursor()");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        evalAndLog("cursor = request.result");
        shouldBe("cursor.key", "'key1'");

        debug("");
        debug("Check expected values:");
        shouldBe("cursor.value.a", "1");
        shouldBe("cursor.value.b", "10");
        shouldBe("cursor.value.foo", "undefined");
    };
    transaction.oncomplete = finishJSTest;
 }
