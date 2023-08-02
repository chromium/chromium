if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB's IDBCursor.continue() behavior when called beyond normal scope.");

var date = new Date();

// we set this pretty high because we get different behavior depending
// if we're in a pre-fetched state or not
self.testLength = 25;

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    self.trans = evalAndLog("trans = event.target.transaction");
    shouldBeNonNull("trans");
    trans.onabort = unexpectedAbortCallback;

    self.objectStore = evalAndLog("db.createObjectStore('someObjectStore')");
    self.indexObject = evalAndLog("objectStore.createIndex('someIndex', 'x')");
    self.nextToAdd = 0;
    addData();
}

function addData()
{
    for (var i=0; i<self.testLength; i++) {
        evalAndLog("objectStore.add({'x': " + i + " }, " + i + ")");
    }
    evalAndLog("continueTest()");
}

function continueTest()
{
    debug("");
    debug("Checking objectStore");
    debug("====================");
    var request = evalAndLog("indexObject.openCursor(null, 'next')");
    evalAndLog("self.continueValue = 0");
    request.onsuccess = evalAndLogCallback("doubleContinueCallback()");
    request.onerror = unexpectedErrorCallback;
    self.stage = 0;
}

function doubleContinueCallback()
{
    evalAndLog("cursor = event.target.result");
    if (cursor) {
        debug("Checking value at " + self.continueValue);
        // data should be valid before calling continue()
        shouldBe("cursor.key", "" + self.continueValue);
        shouldBe("cursor.value.x", "" + self.continueValue);

        // Data should not change during iteration, even if continue() is called and extra time.
        shouldBe("event.target.readyState", "'done'");
        evalAndLog("cursor.continue()");
        shouldBe("event.target.readyState", "'pending'");
        shouldBe("cursor.key", "" + self.continueValue);
        shouldBe("cursor.value.x", "" + self.continueValue);

        // Even if continue is called more than once, the value still shouldn't change.
        evalAndExpectException("cursor.continue()", "DOMException.INVALID_STATE_ERR");
        shouldBe("cursor.key", "" + self.continueValue);
        shouldBe("cursor.value.x", "" + self.continueValue);
        evalAndLog("self.continueValue++;");
    } else {
        evalAndLog("continueIndexTest()");
    }
}

function continueIndexTest()
{
    debug("");
    debug("Checking index");
    debug("==============");
    var request = evalAndLog("indexObject.openCursor(null, 'next')");
    evalAndLog("self.continueValue = 0");
    request.onsuccess = doubleContinueIndexCallback;
    request.onerror = unexpectedErrorCallback;
    self.stage = 0;
}

function doubleContinueIndexCallback()
{
    evalAndLog("cursor = event.target.result");
    if (cursor) {
        debug("Checking value at " + self.continueValue);
        // data should be valid before calling continue()
        shouldBe("cursor.key", "" + self.continueValue);
        shouldBe("cursor.value.x", "" + self.continueValue);

        // Data should not change during iteration, even if continue() is called and extra time.
        evalAndLog("cursor.continue()");
        shouldBe("cursor.key", "" + self.continueValue);
        shouldBe("cursor.value.x", "" + self.continueValue);

        // Even if continue is called more than once, the value still shouldn't change.
        evalAndExpectException("cursor.continue()", "DOMException.INVALID_STATE_ERR");
        shouldBe("cursor.key", "" + self.continueValue);
        shouldBe("cursor.value.x", "" + self.continueValue);
        evalAndLog("self.continueValue++;");
    } else {
        evalAndLog("testModifyContinueOrder()");
    }

}

// Note: This mutates the data
function testModifyContinueOrder()
{
    debug("");
    debug("Checking modification");
    debug("=====================");
    var request = evalAndLog("indexObject.openCursor(null, 'next')");
    evalAndLog("self.continueValue = 0");
    request.onsuccess = modifyContinueOrderCallback;
    request.onerror = unexpectedErrorCallback;
    self.stage = 0;
}

function modifyContinueOrderCallback()
{
    cursor = evalAndLog("cursor = event.target.result");

    self.continueValue++;
    if (cursor) {
        // we sprinkle these checks across the dataset, to sample
        // behavior against pre-fetching. Make sure to use prime
        // numbers for these checks to avoid overlap.
        if (self.continueValue % 2 == 0) {
            // it's ok to call update() and then continue..
            evalAndLog("cursor.update({ x: 100 + self.continueValue })");
            evalAndLog("cursor.continue()");
        } else if (self.continueValue % 3 == 0) {
            // it's ok to call delete() and then continue
            evalAndLog("cursor.delete()");
            evalAndLog("cursor.continue()");
        } else if (self.continueValue % 5 == 0) {
            // it's NOT ok to call continue and then update
            evalAndLog("cursor.continue()");
            evalAndExpectException("cursor.update({ x: 100 + self.continueValue})",
                                   "DOMException.INVALID_STATE_ERR");
        } else if (self.continueValue % 7 == 0) {
            // it's NOT ok to call continue and then delete
            evalAndLog("cursor.continue()");
            evalAndExpectException("cursor.delete()",
                                   "DOMException.INVALID_STATE_ERR");
        } else {
            evalAndLog("cursor.continue()");
        }

    } else {
        finishJSTest();
    }
}
