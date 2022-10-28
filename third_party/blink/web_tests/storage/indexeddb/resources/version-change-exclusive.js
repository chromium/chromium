if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Ensure pending open waits for version change transaction to complete.");

indexedDBTest(prepareDatabase, null, {"runAfterOpen": concurrentOpen});

function concurrentOpen()
{
    debug("calling open() - callback should wait until VERSION_CHANGE transaction is complete");
    var openRequest = evalAndLog("indexedDB.open(dbname)");
    openRequest.onsuccess = openAgainSuccess;
    openRequest.onerror = unexpectedErrorCallback;
}

function prepareDatabase()
{
    db = event.target.result;
    debug("setVersion() callback");
    debug("starting work in VERSION_CHANGE transaction");
    evalAndLog("self.state = 'VERSION_CHANGE started'");

    self.store = evalAndLog("store = db.createObjectStore('test-store')");
    evalAndExpectException("db.transaction('test-store', 'readonly', {durability: 'relaxed'})", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    self.count = 0;
    do_async_puts();

    function do_async_puts()
    {
        var req = evalAndLog("store.put(" + count + ", " + count + ")");
        req.onerror = unexpectedErrorCallback;
        req.onsuccess = function (e) {
            debug("in put's onsuccess");
            if (++self.count < 10) {
                do_async_puts();
            } else {
                debug("ending work in VERSION_CHANGE transaction");
                evalAndLog("self.state = 'VERSION_CHANGE finished'");
            }
        };
    }
}

function openAgainSuccess()
{
    debug("open() callback - this should appear after VERSION_CHANGE transaction ends");
    shouldBeEqualToString("self.state", "VERSION_CHANGE finished");
    finishJSTest();
}
