if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Regression test for IDBRequest issue calling continue on a cursor then aborting.");

indexedDBTest(prepareDatabase, testCursor);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    evalAndLog("db.createObjectStore('store')");
}


function testCursor()
{
    debug("");
    debug("testCursor:");
    evalAndLog("transaction = db.transaction('store', 'readwrite', {durability: 'relaxed'})");

    evalAndLog("store = transaction.objectStore('store')");
    request = evalAndLog("store.add('value1', 'key1')");
    request.onerror = unexpectedErrorCallback;
    request = evalAndLog("store.add('value2', 'key2')");
    request.onerror = unexpectedErrorCallback;

    debug("");
    evalAndLog("state = 0");
    evalAndLog("request = store.openCursor()");
    request.onsuccess = function() {
        debug("");
        debug("'success' event fired at request.");
        shouldBe("++state", "1");
        evalAndLog("request.result.continue()");
        transaction.abort();
    };
    request.onerror = function() {
        debug("");
        debug("'error' event fired at request.");
        shouldBe("++state", "2");
    };
    transaction.oncomplete = unexpectedCompleteCallback;
    transaction.onabort = function() {
        debug("");
        debug("'abort' event fired at transaction.");
        shouldBe("++state", "3");
        finishJSTest();
    };
}
