if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB primary key ordering and readback from cursors.");

indexedDBTest(prepareDatabase, populateStore);
function prepareDatabase()
{
    db = event.target.result;
    evalAndLog("store = db.createObjectStore('store')");
    evalAndLog("index = store.createIndex('index', 'indexKey')");
}

self.keys = [
    "-Infinity",
    "-2",
    "-1",
    "0",
    "1",
    "2",
    "Infinity",

    "'0'",
    "'1'",
    "'2'",
    "'A'",
    "'B'",
    "'C'",
    "'a'",
    "'b'",
    "'c'"
];

function populateStore()
{
    debug("");
    debug("populating store...");
    evalAndLog("trans = db.transaction('store', 'readwrite')");
    evalAndLog("store = trans.objectStore('store');");
    trans.onerror = unexpectedErrorCallback;
    trans.onabort = unexpectedAbortCallback;
    var count = 0;
    var indexKey = 0;
    var keys = self.keys.slice();
    keys.reverse();
    keys.forEach(function(key) {
        var value = { indexKey: indexKey, count: count++ };
        evalAndLog("store.put(" + JSON.stringify(value) + ", " + key + ")");
    });
    trans.oncomplete = checkStore;
}

function checkStore()
{
    debug("");
    debug("iterating cursor...");
    evalAndLog("trans = db.transaction('store', 'readonly')");
    evalAndLog("store = trans.objectStore('store');");
    evalAndLog("index = store.index('index');");
    trans.onerror = unexpectedErrorCallback;
    trans.onabort = unexpectedAbortCallback;
    cursorRequest = evalAndLog("cursorRequest = index.openCursor()");
    evalAndLog("count = 0");
    var indexKey = 0;
    cursorRequest.onerror = unexpectedErrorCallback;
    cursorRequest.onsuccess = function() {
        if (cursorRequest.result) {
            evalAndLog("cursor = cursorRequest.result");
            shouldBe("cursor.key", String(indexKey));
            shouldBe("cursor.primaryKey", self.keys[count++]);
            cursor.continue();
        } else {
            shouldBe("count", "keys.length");
            finishJSTest();
        }
    };
}
