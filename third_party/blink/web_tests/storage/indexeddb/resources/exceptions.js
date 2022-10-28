if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that expected exceptions are thrown per IndexedDB spec.");

indexedDBTest(prepareDatabase, testDatabase);
function prepareDatabase()
{
    db = event.target.result;

    evalAndLog("store = db.createObjectStore('store')");
    evalAndLog("index = store.createIndex('index', 'id')");
    evalAndLog("store.put({id: 'a'}, 1)");
    evalAndLog("store.put({id: 'b'}, 2)");
    evalAndLog("store.put({id: 'c'}, 3)");
    evalAndLog("store.put({id: 'd'}, 4)");
    evalAndLog("store.put({id: 'e'}, 5)");
    evalAndLog("store.put({id: 'f'}, 6)");
    evalAndLog("store.put({id: 'g'}, 7)");
    evalAndLog("store.put({id: 'h'}, 8)");
    evalAndLog("store.put({id: 'i'}, 9)");
    evalAndLog("store.put({id: 'j'}, 10)");
    evalAndLog("otherStore = db.createObjectStore('otherStore')");
    evalAndLog("inlineKeyStore = db.createObjectStore('inlineKeyStore', {keyPath: 'id'})");

    evalAndLog("request = inlineKeyStore.put({id: 0})");
    shouldBeEqualToString("request.readyState", "pending");

    debug("");
    debug("3.2.1 The IDBRequest Interface");

    debug("");
    debug("IDBRequest.error");
    debug("When the done flag is false, getting this property must throw a DOMException of type InvalidStateError.");
    evalAndExpectException("request.error", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    debug("");
    debug("IDBRequest.result");
    debug("When the done flag is false, getting this property must throw a DOMException of type InvalidStateError.");
    evalAndExpectException("request.result", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    debug("");
    debug("3.2.3 Opening a database");

    debug("");
    debug("IDBFactory.cmp()");
    debug("One of the supplied keys was not a valid key.");
    evalAndExpectException("indexedDB.cmp(null, 0)", "0", "'DataError'");
}

function testDatabase()
{
    evalAndLog("db.close()");

    debug("");
    debug("3.2.4 Database");

    request = evalAndLog("indexedDB.open(dbname, 2)");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = checkTransactionAndObjectStoreExceptions;
    request.onupgradeneeded = function() {
        db = request.result;
        var trans = request.transaction;
        trans.onabort = unexpectedAbortCallback;

        debug("");
        debug("IDBDatabase.createObjectStore()");
        debug("If an objectStore with the same name already exists, the implementation must throw a DOMException of type ConstraintError.");
        evalAndExpectException("db.createObjectStore('store')", "0", "'ConstraintError'");
        debug("If keyPath is not a valid key path then a DOMException of type SyntaxError must be thrown.");
        evalAndExpectException("db.createObjectStore('fail', {keyPath: '-invalid-'})", "DOMException.SYNTAX_ERR", "'SyntaxError'");
        debug("If the optionalParameters parameter is specified, and autoIncrement is set to true, and the keyPath parameter is specified to the empty string, or specified to an Array, this function must throw a InvalidAccessError exception.");
        evalAndExpectException("db.createObjectStore('fail', {autoIncrement: true, keyPath: ''})", "DOMException.INVALID_ACCESS_ERR", "'InvalidAccessError'");
        evalAndExpectException("db.createObjectStore('fail', {autoIncrement: true, keyPath: ['a']})", "DOMException.INVALID_ACCESS_ERR", "'InvalidAccessError'");

        debug("");
        debug("IDBDatabase.deleteObjectStore()");
        debug("There is no object store with the given name, compared in a case-sensitive manner, in the connected database.");
        evalAndExpectException("db.deleteObjectStore('no-such-store')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");

        debug("");
        debug("IDBDatabase.transaction()");
        debug('If this method is called on IDBDatabase object for which a "versionchange" transaction is still running, a InvalidStateError exception must be thrown.');
        evalAndExpectException("db.transaction('store', 'readonly', {durability: 'relaxed'})", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    };
}

function checkTransactionAndObjectStoreExceptions()
{
    debug("One of the names provided in the storeNames argument doesn't exist in this database.");
    evalAndExpectException("db.transaction('no-such-store', 'readonly', {durability: 'relaxed'})", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    debug("The value for the mode parameter is invalid.");
    evalAndExpectExceptionClass("db.transaction('store', 'invalid-mode')", "TypeError");
    debug("The 'versionchange' value for the mode parameter can only be set internally during upgradeneeded.");
    evalAndExpectExceptionClass("db.transaction('store', 'versionchange')", "TypeError");
    debug("The function was called with an empty list of store names");
    evalAndExpectException("db.transaction([])", "DOMException.INVALID_ACCESS_ERR", "'InvalidAccessError'");

    debug("");
    debug("One more IDBDatabase.createObjectStore() test:");
    debug('If this function is called from outside a "versionchange" transaction callback ... the implementation must throw a DOMException of type InvalidStateError.');
    evalAndExpectException("db.createObjectStore('fail')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    debug("");
    debug("One more IDBDatabase.deleteObjectStore() test:");
    debug('If this function is called from outside a "versionchange" transaction callback ... the implementation must throw a DOMException of type InvalidStateError.');
    evalAndExpectException("db.deleteObjectStore('fail')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    prepareStoreAndIndex();
}

function prepareStoreAndIndex()
{
    debug("");
    debug("Prepare an object store and index from an inactive transaction for later use.");
    evalAndLog("finishedTransaction = inactiveTransaction = db.transaction('store', 'readonly', {durability: 'relaxed'})");
    inactiveTransaction.onabort = unexpectedAbortCallback;
    evalAndLog("storeFromInactiveTransaction = inactiveTransaction.objectStore('store')");
    evalAndLog("indexFromInactiveTransaction = storeFromInactiveTransaction.index('index')");
    evalAndLog("request = storeFromInactiveTransaction.openCursor()");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        evalAndLog("cursorFromInactiveTransaction = request.result");
    };
    inactiveTransaction.oncomplete = testObjectStore;
}

function testObjectStore()
{
    debug("");
    debug("3.2.5 Object Store");
    evalAndLog("ro_transaction = db.transaction('store', 'readonly', {durability: 'relaxed'})");
    evalAndLog("storeFromReadOnlyTransaction = ro_transaction.objectStore('store')");
    evalAndLog("rw_transaction = db.transaction('store', 'readwrite', {durability: 'relaxed'})");
    evalAndLog("store = rw_transaction.objectStore('store')");

    debug("");
    debug("IDBObjectStore.add()");
    debug('This method throws a DOMException of type ReadOnlyError if the transaction which this IDBObjectStore belongs to is has its mode set to "readonly".');
    evalAndExpectException("storeFromReadOnlyTransaction.add(0, 0)", "0", "'ReadOnlyError'");
    // "If any of the following conditions are true, this method throws a DOMException of type DataError:" - covered in objectstore-basics.html
    debug("The transaction this IDBObjectStore belongs to is not active.");
    evalAndExpectException("storeFromInactiveTransaction.add(0, 0)", "0", "'TransactionInactiveError'");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html
    debug("The data being stored could not be cloned by the internal structured cloning algorithm.");
    evalAndExpectException("store.add(self, 0)", "DOMException.DATA_CLONE_ERR"); // FIXME: Test 'DataCloneError' name when DOM4 exceptions are used in binding.

    debug("");
    debug("IDBObjectStore.clear()");
    debug('This method throws a DOMException of type ReadOnlyError if the transaction which this IDBObjectStore belongs to is has its mode set to "readonly".');
    evalAndExpectException("storeFromReadOnlyTransaction.clear()", "0", "'ReadOnlyError'");
    debug("The transaction this IDBObjectStore belongs to is not active.");
    evalAndExpectException("storeFromInactiveTransaction.clear()", "0", "'TransactionInactiveError'");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html

    debug("");
    debug("IDBObjectStore.count()");
    debug("If the optional key parameter is not a valid key or a key range, this method throws a DOMException of type DataError.");
    evalAndExpectException("store.count({})", "0", "'DataError'");
    debug("The transaction this IDBObjectStore belongs to is not active.");
    evalAndExpectException("storeFromInactiveTransaction.count()", "0", "'TransactionInactiveError'");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html

    debug("");
    debug("IDBObjectStore.delete()");
    debug('This method throws a DOMException of type ReadOnlyError if the transaction which this IDBObjectStore belongs to is has its mode set to "readonly".');
    evalAndExpectException("storeFromReadOnlyTransaction.delete(0)", "0", "'ReadOnlyError'");
    debug("If the key parameter is not a valid key or a key range this method throws a DOMException of type DataError.");
    evalAndExpectException("store.delete({})", "0", "'DataError'");
    debug("The transaction this IDBObjectStore belongs to is not active.");
    evalAndExpectException("storeFromInactiveTransaction.add(0, 0)", "0", "'TransactionInactiveError'");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html

    debug("");
    debug("IDBObjectStore.get()");
    debug("If the key parameter is not a valid key or a key range, this method throws a DOMException of type DataError.");
    evalAndExpectException("store.get({})", "0", "'DataError'");
    debug("The transaction this IDBObjectStore belongs to is not active.");
    evalAndExpectException("storeFromInactiveTransaction.get(0)", "0", "'TransactionInactiveError'");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html

    debug("");
    debug("IDBObjectStore.getAll()");
    debug("If the key parameter is not a valid key or a key range, this method throws a DOMException of type DataError.");
    evalAndExpectException("store.getAll({})", "0", "'DataError'");
    debug("The transaction this IDBObjectStore belongs to is not active.");
    evalAndExpectException("storeFromInactiveTransaction.getAll(0)", "0", "'TransactionInactiveError'");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html

    debug("");
    debug("IDBObjectStore.getAllKeys()");
    debug("If the key parameter is not a valid key or a key range, this method throws a DOMException of type DataError.");
    evalAndExpectException("store.getAllKeys({})", "0", "'DataError'");
    debug("The transaction this IDBObjectStore belongs to is not active.");
    evalAndExpectException("storeFromInactiveTransaction.getAllKeys(0)", "0", "'TransactionInactiveError'");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html
    //
    debug("");
    debug("IDBObjectStore.index()");
    debug("There is no index with the given name, compared in a case-sensitive manner, in the connected database.");
    evalAndExpectException("store.index('no-such-index')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    debug("Occurs if a request is made on a source object that has been deleted or removed, or if the transaction the object store belongs to has finished.");
    evalAndExpectException("storeFromInactiveTransaction.index('index')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html

    debug("");
    debug("IDBObjectStore.openCursor()");
    debug("If the range parameter is specified but is not a valid key or a key range, this method throws a DOMException of type DataError.");
    evalAndExpectException("store.openCursor({})", "0", "'DataError'");
    debug("The transaction this IDBObjectStore belongs to is not active.");
    evalAndExpectException("storeFromInactiveTransaction.openCursor()", "0", "'TransactionInactiveError'");
    debug("The value for the direction parameter is invalid.");
    evalAndExpectExceptionClass("store.openCursor(0, 'invalid-direction')", "TypeError");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html

    debug("");
    debug("IDBObjectStore.openKeyCursor()");
    debug("If the range parameter is specified but is not a valid key or a key range, this method throws a DOMException of type DataError.");
    evalAndExpectException("store.openKeyCursor({})", "0", "'DataError'");
    debug("The transaction this IDBObjectStore belongs to is not active.");
    evalAndExpectException("storeFromInactiveTransaction.openKeyCursor()", "0", "'TransactionInactiveError'");
    debug("The value for the direction parameter is invalid.");
    evalAndExpectExceptionClass("store.openKeyCursor(0, 'invalid-direction')", "TypeError");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html

    debug("");
    debug("IDBObjectStore.put()");
    debug('This method throws a DOMException of type ReadOnlyError if the transaction which this IDBObjectStore belongs to is has its mode set to "readonly".');
    evalAndExpectException("storeFromReadOnlyTransaction.put(0, 0)", "0", "'ReadOnlyError'");
    // "If any of the following conditions are true, this method throws a DOMException of type DataError:" - covered in objectstore-basics.html
    debug("The transaction this IDBObjectStore belongs to is not active.");
    evalAndExpectException("storeFromInactiveTransaction.put(0, 0)", "0", "'TransactionInactiveError'");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html
    debug("The data being stored could not be cloned by the internal structured cloning algorithm.");
    evalAndExpectException("store.put(self, 0)", "DOMException.DATA_CLONE_ERR"); // FIXME: Test 'DataCloneError' name when DOM4 exceptions are used in binding.

    evalAndLog("db.close()");
    evalAndLog("ro_transaction.oncomplete = transactionComplete");
    evalAndLog("rw_transaction.oncomplete = transactionComplete");
}

var numCompleted = 0;
function transactionComplete(evt)
{
    preamble(evt);
    numCompleted++;
    if (numCompleted == 1) {
        debug("First transaction completed");
        return;
    }
    evalAndLog("request = indexedDB.open(dbname, 3)");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    evalAndLog("request.onupgradeneeded = onUpgradeNeeded3");
}

function onUpgradeNeeded3()
{
    db = request.result;
    var trans = request.transaction;
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = testOutsideVersionChangeTransaction;
    store = trans.objectStore('store');

    debug("");
    debug("IDBObjectStore.createIndex()");
    debug("If an index with the same name already exists, the implementation must throw a DOMException of type ConstraintError. ");
    evalAndExpectException("store.createIndex('index', 'keyPath')", "0", "'ConstraintError'");
    debug("If keyPath is not a valid key path then a DOMException of type SyntaxError must be thrown.");
    evalAndExpectException("store.createIndex('fail', '-invalid-')", "DOMException.SYNTAX_ERR", "'SyntaxError'");
    debug("If keyPath is an Array and the multiEntry property in the optionalParameters is true, then a DOMException of type InvalidAccessError must be thrown.");
    evalAndExpectException("store.createIndex('fail', ['a'], {multiEntry: true})", "DOMException.INVALID_ACCESS_ERR", "'InvalidAccessError'");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html

    debug("");
    debug("IDBObjectStore.deleteIndex()");
    debug("There is no index with the given name, compared in a case-sensitive manner, in the connected database.");
    evalAndExpectException("store.deleteIndex('no-such-index')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
}

function testOutsideVersionChangeTransaction() {
    debug("");
    debug("One more IDBObjectStore.createIndex() test:");
    debug('If this function is called from outside a "versionchange" transaction callback ... the implementation must throw a DOMException of type InvalidStateError.');
    evalAndExpectException("db.transaction('store', 'readonly', {durability: 'relaxed'}).objectStore('store').createIndex('fail', 'keyPath')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    debug("");
    debug("One more IDBObjectStore.deleteIndex() test:");
    debug('If this function is called from outside a "versionchange" transaction callback ... the implementation must throw a DOMException of type InvalidStateError.');
    evalAndExpectException("db.transaction('store', 'readonly', {durability: 'relaxed'}).objectStore('store').deleteIndex('fail', 'keyPath')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    testIndex();
}

function testIndex()
{
    debug("");
    debug("3.2.6 Index");
    evalAndLog("indexFromReadOnlyTransaction = db.transaction('store', 'readonly', {durability: 'relaxed'}).objectStore('store').index('index')");
    evalAndLog("index = db.transaction('store', 'readwrite', {durability: 'relaxed'}).objectStore('store').index('index')");

    debug("");
    debug("IDBIndex.count()");
    debug("If the optional key parameter is not a valid key or a key range, this method throws a DOMException of type DataError.");
    evalAndExpectException("index.count({})", "0", "'DataError'");
    debug("The transaction this IDBIndex belongs to is not active.");
    evalAndExpectException("indexFromInactiveTransaction.count()", "0", "'TransactionInactiveError'");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html

    debug("");
    debug("IDBIndex.get()");
    debug("If the key parameter is not a valid key or a key range, this method throws a DOMException of type DataError.");
    evalAndExpectException("index.get({})", "0", "'DataError'");
    debug("The transaction this IDBIndex belongs to is not active.");
    evalAndExpectException("indexFromInactiveTransaction.get(0)", "0", "'TransactionInactiveError'");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html
    //
    debug("");
    debug("IDBIndex.getAll()");
    debug("If the key parameter is not a valid key or a key range, this method throws a DOMException of type DataError.");
    evalAndExpectException("index.getAll({})", "0", "'DataError'");
    debug("The transaction this IDBIndex belongs to is not active.");
    evalAndExpectException("indexFromInactiveTransaction.getAll(0)", "0", "'TransactionInactiveError'");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html

    debug("");
    debug("IDBIndex.getKey()");
    debug("If the key parameter is not a valid key or a key range, this method throws a DOMException of type DataError.");
    evalAndExpectException("index.getKey({})", "0", "'DataError'");
    debug("The transaction this IDBIndex belongs to is not active.");
    evalAndExpectException("indexFromInactiveTransaction.getKey(0)", "0", "'TransactionInactiveError'");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html

    debug("");
    debug("IDBIndex.getAllKeys()");
    debug("If the key parameter is not a valid key or a key range, this method throws a DOMException of type DataError.");
    evalAndExpectException("index.getAllKeys({})", "0", "'DataError'");
    debug("The transaction this IDBIndex belongs to is not active.");
    evalAndExpectException("indexFromInactiveTransaction.getAllKeys(0)", "0", "'TransactionInactiveError'");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html

    debug("");
    debug("IDBIndex.openCursor()");
    debug("If the range parameter is specified but is not a valid key or a key range, this method throws a DOMException of type DataError.");
    evalAndExpectException("index.openCursor({})", "0", "'DataError'");
    debug("The transaction this IDBIndex belongs to is not active.");
    evalAndExpectException("indexFromInactiveTransaction.openCursor()", "0", "'TransactionInactiveError'");
    debug("The value for the direction parameter is invalid.");
    evalAndExpectExceptionClass("index.openCursor(0, 'invalid-direction')", "TypeError");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html

    debug("");
    debug("IDBIndex.openKeyCursor()");
    debug("If the range parameter is specified but is not a valid key or a key range, this method throws a DOMException of type DataError.");
    evalAndExpectException("index.openKeyCursor({})", "0", "'DataError'");
    debug("The transaction this IDBIndex belongs to is not active.");
    evalAndExpectException("indexFromInactiveTransaction.openKeyCursor()", "0", "'TransactionInactiveError'");
    debug("The value for the direction parameter is invalid.");
    evalAndExpectExceptionClass("index.openKeyCursor(0, 'invalid-direction')", "TypeError");
    // "Occurs if a request is made on a source object that has been deleted or removed." - covered in deleted-objects.html

    testCursor();
}

function testCursor()
{
    debug("");
    debug("3.2.7 Cursor");
    evalAndLog("transaction = db.transaction(['store', 'inlineKeyStore'], 'readwrite')");

    makeCursor();

    function makeCursor() {
        evalAndLog("request = transaction.objectStore('store').openCursor()");
        primaryCursorRequest = request;
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            evalAndLog("cursor = request.result");
            request.onsuccess = null;
            makeKeyCursor();
        };
    }

    function makeKeyCursor() {
        evalAndLog("request = transaction.objectStore('store').index('index').openKeyCursor()");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            evalAndLog("keyCursor = request.result");
            request.onsuccess = null;
            makeReverseCursor();
        };
    }

    function makeReverseCursor() {
        evalAndLog("request = transaction.objectStore('store').openCursor(IDBKeyRange.lowerBound(-Infinity), 'prev')");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            evalAndLog("reverseCursor = request.result");
            request.onsuccess = null;
            makeInlineCursor();
        };
    }

    function makeInlineCursor() {
        evalAndLog("request = transaction.objectStore('inlineKeyStore').openCursor()");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            evalAndLog("inlineCursor = request.result");
            request.onsuccess = null;
            testCursorAdvance();
        };
    }

    function testCursorAdvance() {
        debug("");
        debug("IDBCursor.advance()");
        debug("Calling this method more than once before new cursor data has been loaded is not allowed and results in a DOMException of type InvalidStateError being thrown.");
        debug("If the value for count is 0 (zero) or a negative number, this method must throw a JavaScript TypeError exception.");
        evalAndExpectExceptionClass("cursor.advance(0)", "TypeError");
        evalAndLog("cursor.advance(1)");
        evalAndExpectException("cursor.advance(1)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
        debug("The transaction this IDBCursor belongs to is not active.");
        evalAndExpectException("cursorFromInactiveTransaction.advance(1)", "0", "'TransactionInactiveError'");
        primaryCursorRequest.onsuccess = testCursorContinue;
    }

    function testCursorContinue() {
        debug("");
        debug("IDBCursor.continue()");
        debug("The parameter is not a valid key.");
        evalAndExpectException("cursor.continue({})", "0", "'DataError'");
        debug("The parameter is less than or equal to this cursor's position and this cursor's direction is \"next\" or \"nextunique\".");
        evalAndExpectException("cursor.continue(-Infinity)", "0", "'DataError'");
        debug("The parameter is greater than or equal to this cursor's position and this cursor's direction is \"prev\" or \"prevunique\".");
        evalAndExpectException("reverseCursor.continue(100)", "0", "'DataError'");
        debug("Calling this method more than once before new cursor data has been loaded is not allowed and results in a DOMException of type InvalidStateError being thrown.");
        evalAndLog("cursor.continue()");
        evalAndExpectException("cursor.continue()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
        debug("The transaction this IDBCursor belongs to is not active.");
        evalAndExpectException("cursorFromInactiveTransaction.continue()", "0", "'TransactionInactiveError'");
        testCursorDelete();
    }

    function testCursorDelete() {
        debug("");
        debug("IDBCursor.delete()");
        debug("If this cursor's got value flag is false, or if this cursor was created using openKeyCursor a DOMException of type InvalidStateError is thrown.");
        evalAndExpectException("keyCursor.delete()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
        debug("The transaction this IDBCursor belongs to is not active.");
        evalAndExpectException("cursorFromInactiveTransaction.delete()", "0", "'TransactionInactiveError'");
        primaryCursorRequest.onsuccess = testCursorUpdate;
    }

    function testCursorUpdate() {
        debug("");
        debug("IDBCursor.update()");
        debug("If this cursor's got value flag is false or if this cursor was created using openKeyCursor. This method throws a DOMException of type InvalidStateError.");
        evalAndExpectException("keyCursor.update({})", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
        debug("If the effective object store of this cursor uses in-line keys and evaluating the key path of the value parameter results in a different value than the cursor's effective key, this method throws a DOMException of type DataError.");
        evalAndExpectException("inlineCursor.update({id: 1})", "0", "'DataError'");
        debug("If the structured clone algorithm throws an exception, that exception is rethrown.");
        evalAndExpectException("cursor.update(self)", "DOMException.DATA_CLONE_ERR"); // FIXME: Test 'DataCloneError' name when DOM4 exceptions are used in binding.
        debug("The transaction this IDBCursor belongs to is not active.");
        evalAndExpectException("cursorFromInactiveTransaction.update({})", "0", "'TransactionInactiveError'");

        primaryCursorRequest.onsuccess = null;
        makeReadOnlyCursor();
    }

    // Can't have both transactions running at once, so these tests must be separated out.
    function makeReadOnlyCursor() {
        evalAndLog("readOnlyTransaction = db.transaction('store', 'readonly', {durability: 'relaxed'})");
        evalAndLog("request = readOnlyTransaction.objectStore('store').openCursor()");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            evalAndLog("cursorFromReadOnlyTransaction = request.result");
            doReadOnlyCursorTests();
        };
    }

    function doReadOnlyCursorTests() {
       debug("");
       debug("One more IDBCursor.delete() test:");
       debug('This method throws a DOMException of type ReadOnlyError if the transaction which this IDBCursor belongs to has its mode set to "readonly".');
       evalAndExpectException("cursorFromReadOnlyTransaction.delete()", "0", "'ReadOnlyError'");

       debug("");
       debug("One more IDBCursor.update() test:");
       debug('This method throws a DOMException of type ReadOnlyError if the transaction which this IDBCursor belongs to has its mode set to "readonly".');
       evalAndExpectException("cursorFromReadOnlyTransaction.update({})", "0", "'ReadOnlyError'");

       testTransaction();
    }
}

function testTransaction()
{
    debug("");
    debug("3.2.8 Transaction");

    debug("");
    debug("IDBTransaction.abort()");
    debug("If this transaction is finished, throw a DOMException of type InvalidStateError. ");
    evalAndExpectException("finishedTransaction.abort()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    debug("If the requested object store is not in this transaction's scope.");
    evalAndExpectException("db.transaction('store', 'readonly', {durability: 'relaxed'}).objectStore('otherStore')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");

    finishJSTest();
}
