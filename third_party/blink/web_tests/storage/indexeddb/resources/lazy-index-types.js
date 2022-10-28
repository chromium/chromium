if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test lazy IndexedDB index population with various key types.");

function test()
{
    setDBNameFromPath();

    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = function() {
        request = evalAndLog("indexedDB.open(dbname, 1)");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = unexpectedBlockedCallback;
        request.onupgradeneeded = onUpgradeNeeded;
        request.onsuccess = onSuccess;
    };
}

function onUpgradeNeeded(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store', {autoIncrement: true})");
    evalAndLog("index = store.createIndex('greedy-index', 'id')");

    [
        // Valid key types:
        "0",
        "new Date(0)",
        "'string'",
        "[]",

        // Types in arrays, for good measure:
        "[0]",
        "[new Date(0)]",
        "['string']",
        "[[]]",

        // Types which are cloneable but not valid keys:
        "undefined",
        "null",
        "true",
        "false",
        "{}",
        "/(?:)/"
    ].forEach(function(indexKey) {
        evalAndLog("store.put({id: " + indexKey + "})");
    });

    evalAndLog("index = store.createIndex('lazy-index', 'id')");
    evalAndLog("expectedIndexSize = 8");
}

function onSuccess(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("trans = db.transaction('store', 'readonly', {durability: 'relaxed'})");
    trans.onabort = unexpectedAbortCallback;
    evalAndLog("store = trans.objectStore('store')");

    evalAndLog("greedyIndex = store.index('greedy-index')");
    gotGreedyCount = false;
    evalAndLog("request = greedyIndex.count()");
    request.onsuccess = function countSuccess(evt) {
        preamble(evt);
        shouldBe("event.target.result", "expectedIndexSize");
        evalAndLog("gotGreedyCount = true");
    };

    evalAndLog("lazyIndex = store.index('lazy-index')");
    gotLazyCount = false;
    evalAndLog("request = lazyIndex.count()");
    request.onsuccess = function countSuccess(evt) {
        preamble(evt);
        shouldBe("event.target.result", "expectedIndexSize");
        evalAndLog("gotLazyCount = true");
    };

    trans.oncomplete = onComplete;
}

function onComplete(evt)
{
    preamble(evt);

    shouldBeTrue("gotGreedyCount");
    shouldBeTrue("gotLazyCount");
    finishJSTest();
}

test();
