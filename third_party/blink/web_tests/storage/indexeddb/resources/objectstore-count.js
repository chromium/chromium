if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB's IDBObjectStore.count().");

indexedDBTest(prepareDatabase, verifyCount);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

    store = evalAndLog("store = db.createObjectStore('storeName', null)");

    debug("adding 0 ... 99");
    for (var i = 0; i < 100; ++i) {
        request = store.add(i, i);
        request.onerror = unexpectedErrorCallback;
    }
}

function verifyCount()
{
    debug("");
    debug("verifying count without range");
    trans = evalAndLog("trans = db.transaction('storeName', 'readonly')");
    shouldBeNonNull("trans");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = verifyCountWithRange;

    store = evalAndLog("store = trans.objectStore('storeName')");
    shouldBeNonNull("store");

    request = evalAndLog("request = store.count()");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
         shouldBeEqualToString("typeof request.result", "number");
         shouldBe("request.result", "100");
         // let the transaction complete
    };
}

function verifyCountWithRange()
{
    debug("");
    debug("verifying count with range");
    trans = evalAndLog("trans = db.transaction('storeName', 'readonly')");
    shouldBeNonNull("trans");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = verifyCountWithKey;

    store = evalAndLog("store = trans.objectStore('storeName')");
    shouldBeNonNull("store");

    var tests = [
        { lower: 0, lowerOpen: false, upper: 99, upperOpen: false, expected: 100 },
        { lower: 0, lowerOpen: true, upper: 99, upperOpen: false, expected: 99 },
        { lower: 0, lowerOpen: false, upper: 99, upperOpen: true, expected: 99 },
        { lower: 0, lowerOpen: true, upper: 99, upperOpen: true, expected: 98 },
        { lower: 0, lowerOpen: false, upper: 100, upperOpen: false, expected: 100 },
        { lower: 0, lowerOpen: false, upper: 100, upperOpen: false, expected: 100 },
        { lower: 10, lowerOpen: false, upper: 100, upperOpen: false, expected: 90 },
        { lower: 0, lowerOpen: false, upper: 0, upperOpen: false, expected: 1 },
        { lower: 500, lowerOpen: false, upper: 500, upperOpen: false, expected: 0 }
    ];

    function nextTest() {
        debug("");
        evalAndLog("test = " + JSON.stringify(tests.shift()));
        request = evalAndLog("request = store.count(IDBKeyRange.bound(test.lower, test.upper, test.lowerOpen, test.upperOpen))");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
             shouldBeEqualToString("typeof request.result", "number");
             shouldBe("request.result", String(test.expected));

             if (tests.length) {
                 nextTest();
             }
             // otherwise let the transaction complete
        };
    }

    nextTest();
}

function verifyCountWithKey()
{
    debug("");
    debug("verifying count with key");
    trans = evalAndLog("trans = db.transaction('storeName', 'readonly')");
    shouldBeNonNull("trans");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = finishJSTest;

    store = evalAndLog("store = trans.objectStore('storeName')");
    shouldBeNonNull("store");

    evalAndExpectException("store.count(NaN)", "0", "'DataError'");
    evalAndExpectException("store.count({})", "0", "'DataError'");
    evalAndExpectException("store.count(/regex/)", "0", "'DataError'");

    var tests = [
        { key: 0, expected: 1 },
        { key: 100, expected: 0 },
        { key: null, expected: 100 }
    ];

    function nextTest() {
        debug("");
        evalAndLog("test = " + JSON.stringify(tests.shift()));
        request = evalAndLog("request = store.count(test.key)");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            shouldBeEqualToString("typeof request.result", "number");
            shouldBe("request.result", String(test.expected));
             if (tests.length) {
                 nextTest();
             }
             // otherwise let the transaction complete
        };
    }

    nextTest();
}
