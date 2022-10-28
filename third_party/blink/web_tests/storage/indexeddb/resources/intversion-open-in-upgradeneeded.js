if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test calling db.open in upgradeneeded.");

function test()
{
    setDBNameFromPath();

    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onsuccess = deleteSuccess;
    request.onerror = unexpectedErrorCallback;
}

function deleteSuccess(evt) {
    evalAndLog("request = indexedDB.open(dbname, 1)");
    evalAndLog("request.onupgradeneeded = upgradeNeeded1");
    evalAndLog("request.onsuccess = openSuccess1");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
}

var sawTransactionComplete = false;
function upgradeNeeded1(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    shouldBe("event.newVersion", "1");

    evalAndLog("transaction = event.target.transaction");
    evalAndLog("db.createObjectStore('os')");
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = function transactionOnComplete() {
        preamble();
        evalAndLog("sawTransactionComplete = true");
    };
    evalAndLog("db.onversionchange = onVersionChange");
    evalAndLog("request = indexedDB.open(dbname, 3)");
    evalAndLog("request.onupgradeneeded = upgradeNeeded2");
    evalAndLog("request.onsuccess = openSuccess2");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess1(evt)
{
    preamble(evt);
    shouldBeTrue("sawTransactionComplete");
    db = evalAndLog("db = event.target.result");
    shouldBe('db.version', "1");
    debug("Start a transaction to ensure the connection is still open.");
    evalAndLog("transaction = db.transaction('os', 'readonly', {durability: 'relaxed'})");
}

function onVersionChange(evt)
{
    preamble(evt);
    evalAndLog("db.close()");
}

function upgradeNeeded2(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    shouldBe("event.newVersion", "3");
}

function openSuccess2(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    shouldBe("db.version", "3");
    finishJSTest();
}

test();
