if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that when db.close is called in upgradeneeded, the db is cleaned up on refresh.");

function test()
{
    setDBNameFromPath();

    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onsuccess = deleteSuccess;
    request.onerror = unexpectedErrorCallback;
}

function deleteSuccess(evt) {
    evalAndLog("request = indexedDB.open(dbname, 7)");
    request.onsuccess = unexpectedSuccessCallback;
    request.onupgradeneeded = upgradeNeeded;
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = openError;
    debug("");
}

var sawTransactionComplete = false;
function upgradeNeeded(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    shouldBe("event.newVersion", "7");

    evalAndLog("transaction = event.target.transaction");
    evalAndLog("db.createObjectStore('os')");
    evalAndLog("db.close()");
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = function() {
        debug("");
        debug("transaction.oncomplete:");
        evalAndLog("sawTransactionComplete = true");
    };
}

function openError(evt)
{
    preamble(evt);
    shouldBeTrue("sawTransactionComplete");

    shouldBe("event.target.error.name", "'AbortError'");
    shouldBe("event.result", "undefined");

    debug("");
    debug("Verify that the old connection is unchanged and was closed:");
    shouldBeNonNull("db");
    shouldBe('db.version', "7");
    evalAndExpectException("db.transaction('os', 'readonly', {durability: 'relaxed'})", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    finishJSTest();
}

test();
