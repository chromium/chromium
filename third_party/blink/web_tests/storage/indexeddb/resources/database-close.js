if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB 'steps for closing a database connection'");

indexedDBTest(prepareDatabase, testClose);
function prepareDatabase()
{
    connection = event.target.result;
    evalAndLog("store = connection.createObjectStore('store')");
    evalAndLog("store.put('value1', 'key1')");
    evalAndLog("store.put('value2', 'key2')");
}

function testClose()
{
    debug("");
    debug("testClose():");

    debug("Create transactions using connection:");
    evalAndLog("trans1 = connection.transaction('store', 'readonly', {durability: 'relaxed'})");
    trans1.onabort = unexpectedAbortCallback;

    evalAndLog("trans2 = connection.transaction('store', 'readonly', {durability: 'relaxed'})");
    trans2.onabort = unexpectedAbortCallback;

    debug("");
    debug("Close the connection:");
    evalAndLog("connection.close()");
    debug("Step 1: Set the internal closePending flag of connection to true. [Verified via side effects, below.]");

    debug("");
    debug("Step 2: Wait for all transactions created using connection to complete. Once they are complete, connection is closed.");
    evalAndLog("awaiting_transaction_count = 2");
    function transactionCompleted() {
        awaiting_transaction_count -= 1;

        if (awaiting_transaction_count == 0) {
            debug("");
            debug("All transactions completed - database deletes should now be unblocked.");
        }
    }
    request = evalAndLog("trans1.objectStore('store').get('key1')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(e) {
        event = e;
        debug("");
        debug("transaction #1 request successful");
        shouldBeEqualToString("event.target.result", "value1");
    };
    trans1.oncomplete = transactionCompleted;

    request = evalAndLog("trans2.objectStore('store').get('key2')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(e) {
        event = e;
        debug("");
        debug("transaction #2 request successful");
        shouldBeEqualToString("event.target.result", "value2");
    };
    trans2.oncomplete = transactionCompleted;

    debug("");
    debug("NOTE: Once the closePending flag has been set to true no new transactions can be created using connection. All functions that create transactions first check the closePending flag first and throw an exception if it is true.");
    debug("");
    evalAndExpectException("trans3 = connection.transaction('store', 'readonly', {durability: 'relaxed'})", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    debug("");
    debug("NOTE: Once the connection is closed, this can unblock the steps for deleting a database, which waits for connections to a given database to be closed before continuing.");
    debug("");

    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        debug("");
        debug("delete database unblocked");
        shouldBe("awaiting_transaction_count", "0");
        finishJSTest();
    };
}
