if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB opening database connections during transactions");

indexedDBTest(prepareDatabase, startTransaction);
function prepareDatabase()
{
    dbc1 = event.target.result;
    evalAndLog("dbc1.createObjectStore('storeName')");
    event.target.transaction.oncomplete = function (e) {
        debug("database preparation complete");
        debug("");
    };
}

function startTransaction()
{
    debug("starting transaction");
    evalAndLog("state = 'starting'");
    evalAndLog("trans = dbc1.transaction('storeName', 'readwrite', {durability: 'relaxed'})");

    debug("the transaction is kept alive with a series of puts until opens are complete");
    (function keepAlive() {
        // Don't log, since this may run an arbitrary number of times.
        if (state !== 'open3complete') {
            var request = trans.objectStore('storeName').put('value', 'key');
            request.onerror = unexpectedErrorCallback;
            request.onsuccess = keepAlive;
        }
    }());

    trans.onabort = unexpectedAbortCallback;
    trans.onerror = unexpectedErrorCallback;
    trans.oncomplete = function (e) {
        debug("transaction complete");
        shouldBeEqualToString("state", "open3complete");
        debug("");
        finishJSTest();
    };

    debug("");
    tryOpens();
}

function tryOpens()
{
    debug("trying to open the same database");
    evalAndLog("openreq2 = indexedDB.open(dbname)");
    openreq2.onerror = unexpectedErrorCallback;
    openreq2.onsuccess = function (e) {
        debug("openreq2.onsuccess");
        shouldBeEqualToString("state", "starting");
        evalAndLog("state = 'open2complete'");
        debug("");
    };
    debug("");

    debug("trying to open a different database");
    evalAndLog("openreq3 = indexedDB.open(dbname + '2')");
    openreq3.onerror = unexpectedErrorCallback;
    openreq3.onsuccess = function (e) {
        debug("openreq3.onsuccess");
        shouldBeEqualToString("state", "open2complete");
        evalAndLog("state = 'open3complete'");
        debug("");
    };
    debug("");
}
