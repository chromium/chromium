if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB workers, recursion, and transaction termination.");

indexedDBTest(prepareDatabase, createTransaction);
function prepareDatabase(evt)
{
    preamble(evt);
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    evalAndLog("db.createObjectStore('store')");
}

function createTransaction()
{
    debug("");
    debug("createTransaction():");
    evalAndLog("transaction = db.transaction('store', 'readonly', {durability: 'relaxed'})");
    evalAndLog("store = transaction.objectStore('store')");
    transaction.onerror = unexpectedErrorCallback;
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = emptyTransactionCompleted;
}

function emptyTransactionCompleted()
{
    testPassed("Transaction completed");
    evalAndExpectException("store.get(0)", "0", "'TransactionInactiveError'");
    recursionTest();
}

function recursionTest()
{
    debug("");
    debug("recursionTest():");
    evalAndLog("transaction = db.transaction('store', 'readonly', {durability: 'relaxed'})");
    evalAndLog("store = transaction.objectStore('store')");
    transaction.oncomplete = transactionCompleted;
    transaction.onabort = unexpectedAbortCallback;
    evalAndLog("store.get(0)");
    testPassed("transaction is active");
    recurse(1);
}

function recurse(count)
{
    debug("recursion depth: " + count);
    evalAndLog("store.get(0)");
    testPassed("transaction is still active");
    if (count < 3) {
        recurse(count + 1);
    }
    debug("recursion depth: " + count);
    evalAndLog("store.get(0)");
    testPassed("transaction is still active");
}

function transactionCompleted()
{
    testPassed("transaction completed");
    evalAndExpectException("store.get(0)", "0", "'TransactionInactiveError'");

    debug("");
    debug("trying a timeout callback:");
    evalAndLog("setTimeout(timeoutTest, 0)");
}

function timeoutTest()
{
    debug("");
    debug("timeoutTest():");

    evalAndLog("transaction = db.transaction('store', 'readonly', {durability: 'relaxed'})");
    evalAndLog("store = transaction.objectStore('store')");
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = function () {
        testPassed("transaction started in setTimeout() callback completed");
        evalAndExpectException("store.get(0)", "0", "'TransactionInactiveError'");

        errorTest();
    };
}

function errorTest()
{
    debug("");
    debug("errorTest():");
    evalAndLog("self.old_onerror = self.onerror");
    evalAndLog("self.onerror = errorHandler");
    throw new Error("ignore this");
}

function errorHandler(e)
{
    debug("");
    debug("errorHandler():");
    // FIXME: Should be able to stop the error here, but it isn't an Event object.
    // evalAndLog("event.preventDefault()");
    evalAndLog("self.onerror = self.old_onerror");
    evalAndLog("transaction = db.transaction('store', 'readonly', {durability: 'relaxed'})");
    evalAndLog("store = transaction.objectStore('store')");
    transaction.onerror = unexpectedErrorCallback;
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = errorTransactionCompleted;
}

function errorTransactionCompleted()
{
    testPassed("Transaction completed");
    evalAndExpectException("store.get(0)", "0", "'TransactionInactiveError'");
    finishJSTest();
}
