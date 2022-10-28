if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB transaction does not crash on abort.");

indexedDBTest(prepareDatabase, setVersionComplete);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    evalAndLog("db.createObjectStore('foo')");
}

function setVersionComplete()
{
    evalAndLog("db.transaction('foo', 'readonly', {durability: 'relaxed'})");
    evalAndLog("self.gc()");
    finishJSTest();
}
