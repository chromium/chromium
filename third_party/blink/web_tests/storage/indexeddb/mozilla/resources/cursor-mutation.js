// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_cursor_mutation.html?force=1
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's cursor mutation");

indexedDBTest(prepareDatabase, checkCursorResults);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    debug("");
    debug("setupObjectStoreAndCreateIndex():");

    objectStore = evalAndLog("objectStore = db.createObjectStore('foo', { keyPath: 'ss' })");
    index = evalAndLog("index = objectStore.createIndex('name', 'name', { unique: true })");
    objectStoreData = evalAndLog("objectStoreData = [\n" + 
         // To be removed.
"        { ss: '237-23-7732', name: 'Bob' },\n" + 

         // Always present.
"        { ss: '237-23-7733', name: 'Ann' },\n" +
"        { ss: '237-23-7734', name: 'Ron' },\n" +
"        { ss: '237-23-7735', name: 'Sue' },\n" +
"        { ss: '237-23-7736', name: 'Joe' },\n" +

         // To be added.
"        { ss: '237-23-7737', name: 'Pat' }\n" +
"    ]");

    for (i = 0; i < objectStoreData.length - 1; i++) {
        evalAndLog("objectStore.add(objectStoreData[" + i + "])");
    }

    setupCursor();
}

function setupCursor()
{
    debug("");
    debug("setupCursor():");

    count = evalAndLog("count = 0");
    sawAdded = evalAndLog("sawAdded = false");
    sawRemoved = evalAndLog("sawRemoved = false");

    request = evalAndLog("request = objectStore.openCursor()");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = iterateCursor;
}

function iterateCursor()
{
    debug("");
    debug("iterateCursor():");
    cursor = evalAndLog("cursor = event.target.result");
    if (cursor) {
        shouldBeEqualToString("cursor.value.name", objectStoreData[count].name);
        if (cursor.value.name == objectStoreData[0].name) {
            sawRemoved = evalAndLog("sawRemoved = true");
        }
        if (cursor.value.name == objectStoreData[objectStoreData.length - 1].name) {
            sawAdded = evalAndLog("sawAdded = true");
        }
        evalAndLog("count++");
        evalAndLog("cursor.continue()");
    }
}

function checkCursorResults()
{
    debug("");
    debug("checkCursorResults():");
    shouldBe("count", "objectStoreData.length - 1");
    shouldBe("sawAdded", "false");
    shouldBe("sawRemoved", "true");

    setupMutatingCursor();
}

function setupMutatingCursor()
{
    debug("");
    debug("setupMutatingCursor():");
   
    count = evalAndLog("count = 0");
    sawAdded = evalAndLog("sawAdded = false");
    sawRemoved = evalAndLog("sawRemoved = false");
    debug("[objectStoreDataNameSort is an array of indexes into objectStoreData in alphabetical order by name]");
    objectStoreDataNameSort = evalAndLog("objectStoreDataNameSort = [ 1, 4, 5, 2, 3 ]");

    debug("");

    trans = evalAndLog("trans = db.transaction('foo', 'readwrite')");
    objectStore = evalAndLog("objectStore = trans.objectStore('foo')");
    request = evalAndLog("request = objectStore.index('name').openCursor()");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = iterateMutatingCursor;
    evalAndLog("trans.oncomplete = checkMutatingCursorResults");
}

function iterateMutatingCursor()
{
    debug("");
    debug("iterateMutatingCursor():");
    cursor = evalAndLog("cursor = event.target.result");
    if (cursor) {
        shouldBeEqualToString("cursor.value.name", objectStoreData[objectStoreDataNameSort[count]].name);
        if (cursor.value.name == objectStoreData[0].name) {
            sawRemoved = evalAndLog("sawRemoved = true");
        }
        if (cursor.value.name == objectStoreData[objectStoreData.length - 1].name) {
            sawAdded = evalAndLog("sawAdded = true");
        }
        evalAndLog("count++");

        if (count == 1) {
            debug("");
            debug("Mutating the object store:");

            debug("Removing " + objectStoreData[0].name);
            request = evalAndLog("request = objectStore.delete(objectStoreData[0].ss)");
            request.onerror = unexpectedErrorCallback;
            request.onsuccess = addFinalData;

        } else {
            cursor.continue();
        }
    }
}

function addFinalData()
{
    debug("");
    debug("addFinalData():");
    debug("Adding " + objectStoreData[objectStoreData.length - 1].name);
    request = evalAndLog("request = objectStore.add(objectStoreData[objectStoreData.length - 1])");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function () {
        cursor.continue();
    }
}

function checkMutatingCursorResults()
{
    debug("");
    debug("checkMutatingCursorResults():");
    shouldBe("count", "objectStoreData.length - 1");
    shouldBe("sawAdded", "true");
    shouldBe("sawRemoved", "false");
    finishJSTest();
}
