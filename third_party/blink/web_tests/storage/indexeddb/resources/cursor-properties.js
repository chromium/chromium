if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that IndexedDB's cursor key/primaryKey/value properties preserve mutations.");

function test()
{
    setDBNameFromPath();

    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        request = evalAndLog("indexedDB.open(dbname, 1)");
        request.onblocked = unexpectedBlockedCallback;
        request.onerror = unexpectedErrorCallback;
        request.onupgradeneeded = onUpgradeNeeded;
        request.onsuccess = onOpenSuccess;
    };
}

function onUpgradeNeeded(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");
    evalAndLog("index = store.createIndex('index', 'id')");
    evalAndLog("store.put({id: ['indexKey']}, ['primaryKey'])");
}

function onOpenSuccess(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("trans = db.transaction('store', 'readonly', {durability: 'relaxed'})");
    trans.onabort = unexpectedAbortCallback;
    evalAndLog("store = trans.objectStore('store')");
    evalAndLog("index = store.index('index')");

    debug("");
    evalAndLog("request = index.openCursor()");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function onCursorSuccess(evt) {
        preamble(evt);
        evalAndLog("cursor = event.target.result");
        shouldBeNonNull("cursor");
        shouldBeTrue("areArraysEqual(cursor.key, ['indexKey'])");
        shouldBeTrue("areArraysEqual(cursor.primaryKey, ['primaryKey'])");
        checkProperty("cursor.key");
        checkProperty("cursor.primaryKey");
        checkProperty("cursor.value");
    };

    trans.oncomplete = finishJSTest;
}

function checkProperty(property)
{
    debug("");

    debug("Check identity:");
    evalAndLog("v = " + property);
    shouldBeTrue("v === " + property);

    debug("Check read-only:");
    evalAndLog(property + " = null");
    shouldBeTrue("v === " + property);

    debug("Check mutability:");
    evalAndLog(property + ".expando = 123");
    shouldBe(property + ".expando", "123");
}

test();
