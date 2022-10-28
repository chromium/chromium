if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB's transaction and objectStore calls");

indexedDBTest(prepareDatabase, created);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

    evalAndLog("db.createObjectStore('a')");
    evalAndLog("db.createObjectStore('b')");
    evalAndLog("db.createObjectStore('store').createIndex('index', 'some_path')");
    debug("");
}

function created()
{
    trans = evalAndLog("trans = db.transaction(['a'])");
    evalAndLog("trans.objectStore('a')");
    evalAndExpectException("trans.objectStore('b')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    evalAndExpectException("trans.objectStore('x')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    debug("");

    trans = evalAndLog("trans = db.transaction(['a'])");
    evalAndLog("trans.objectStore('a')");
    evalAndExpectException("trans.objectStore('b')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    evalAndExpectException("trans.objectStore('x')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    debug("");

    trans = evalAndLog("trans = db.transaction(['b'])");
    evalAndLog("trans.objectStore('b')");
    evalAndExpectException("trans.objectStore('a')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    evalAndExpectException("trans.objectStore('x')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    debug("");

    trans = evalAndLog("trans = db.transaction(['a', 'b'])");
    evalAndLog("trans.objectStore('a')");
    evalAndLog("trans.objectStore('b')");
    evalAndExpectException("trans.objectStore('x')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    debug("");

    trans = evalAndLog("trans = db.transaction(['b', 'a'])");
    evalAndLog("trans.objectStore('a')");
    evalAndLog("trans.objectStore('b')");
    evalAndExpectException("trans.objectStore('x')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    debug("");

    debug("Passing a string as the first argument is a shortcut for just one object store:");
    trans = evalAndLog("trans = db.transaction('a', 'readonly', {durability: 'relaxed'})");
    evalAndLog("trans.objectStore('a')");
    evalAndExpectException("trans.objectStore('b')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    evalAndExpectException("trans.objectStore('x')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    debug("");

    shouldThrow("trans = db.transaction()");
    debug("");

    evalAndExpectException("db.transaction(['x'])", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    evalAndExpectException("db.transaction(['x'])", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    evalAndExpectException("db.transaction(['a', 'x'])", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    evalAndExpectException("db.transaction(['x', 'x'])", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    evalAndExpectException("db.transaction(['a', 'x', 'b'])", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    debug("");

    debug("Exception thrown when no stores specified:");
    evalAndExpectException("db.transaction([])", "DOMException.INVALID_ACCESS_ERR");
    debug("");

    debug("{} coerces to a string - so no match, but not a type error:");
    evalAndExpectException("db.transaction({})", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    evalAndExpectException("db.transaction({mode:0})", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    debug("");

    debug("Overriding the default string coercion makes these work:");
    evalAndLog("db.transaction({toString:function(){return 'a';}})");
    evalAndLog("db.transaction([{toString:function(){return 'a';}}])");
    debug("... but you still need to specify a real store:");
    evalAndExpectException("db.transaction([{toString:function(){return 'x';}}])", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    evalAndExpectException("db.transaction([{toString:function(){return 'x';}}])", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    debug("");

    trans = evalAndLog("trans = db.transaction(['store'])");
    shouldBeNonNull("trans");
    trans.onabort = unexpectedAbortCallback;
    trans.onerror = unexpectedErrorCallback;
    trans.oncomplete = afterComplete;
    evalAndLog("store = trans.objectStore('store')");
    shouldBeNonNull("store");
    evalAndLog("store.get('some_key')");
}

function afterComplete()
{
    debug("transaction complete, ensuring methods fail");
    shouldBeNonNull("trans");
    shouldBeNonNull("store");
    evalAndExpectException("trans.objectStore('store')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("store.index('index')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    finishJSTest();
}
