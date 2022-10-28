if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test the basics of IndexedDB's IDBDatabase.");

indexedDBTest(prepareDatabase, testSetVersionAbort);
function prepareDatabase()
{
    db = event.target.result;
    debug("Test that you can't open a transaction while in a versionchange transaction");
    evalAndExpectException('db.transaction("doesntExist", "readonly", {durability: "relaxed"})',
                           "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    shouldBe("db.version", "1");
    shouldBeEqualToString("db.name", dbname);
    shouldBe("db.objectStoreNames", "[]");
    shouldBe("db.objectStoreNames.length", "0");
    shouldBe("db.objectStoreNames.contains('')", "false");
    shouldBeUndefined("db.objectStoreNames[0]");
    shouldBeNull("db.objectStoreNames.item(0)");

    objectStore = evalAndLog('db.createObjectStore("test123")');
    checkObjectStore();
}

function checkObjectStore()
{
    shouldBe("db.objectStoreNames", "['test123']");
    shouldBe("db.objectStoreNames.length", "1");
    shouldBe("db.objectStoreNames.contains('')", "false");
    shouldBe("db.objectStoreNames.contains('test456')", "false");
    shouldBe("db.objectStoreNames.contains('test123')", "true");
}


function testSetVersionAbort()
{
    evalAndLog("db.close()");
    evalAndLog("request = indexedDB.open(dbname, 2)");
    request.onupgradeneeded = createAnotherObjectStore;
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = unexpectedSuccessCallback;
}

function createAnotherObjectStore()
{
    evalAndLog("db = event.target.result");
    shouldBe("db.version", "2");
    shouldBeEqualToString("db.name", dbname);
    checkObjectStore();

    objectStore = evalAndLog('db.createObjectStore("test456")');
    var setVersionTrans = evalAndLog("setVersionTrans = event.target.transaction");
    shouldBeNonNull("setVersionTrans");
    setVersionTrans.oncomplete = unexpectedCompleteCallback;
    setVersionTrans.onabort = checkMetadata;
    evalAndLog("setVersionTrans.abort()");
}

function checkMetadata()
{
    shouldBe("db.version", "1");
    checkObjectStore();
    testClose();
}

function testClose()
{
    evalAndLog("db.close()");
    debug("Now that the connection is closed, transaction creation should fail");
    evalAndExpectException("db.transaction('test123', 'readonly', {durability: 'relaxed'})", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    debug("Call twice, make sure it's harmless");
    evalAndLog("db.close()");
    finishJSTest();
}
