// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_cursor_mutation.html?force=1
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's cursor mutation during object store cursor iteration");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    objectStore = evalAndLog("objectStore = db.createObjectStore('foo', { keyPath: 'ss' });");
    objectStoreData = evalAndLog("objectStoreData = [\n" + 
"        { ss: '237-23-7732', name: 'Bob' },\n" + 

"        { ss: '237-23-7733', name: 'Ann' },\n" +
"        { ss: '237-23-7734', name: 'Ron' },\n" +
"        { ss: '237-23-7735', name: 'Sue' },\n" +
"        { ss: '237-23-7736', name: 'Joe' },\n" +

"        { ss: '237-23-7737', name: 'Pat' }\n" +
"    ];");

    for (i = 0; i < objectStoreData.length - 1; i++) {
        evalAndLog("objectStore.add(objectStoreData[i]);");
    }

    count = evalAndLog("count = 0;");
    sawAdded = evalAndLog("sawAdded = false;");
    sawRemoved = evalAndLog("sawRemoved = false;");

    request = evalAndLog("request = objectStore.openCursor();");
    request.onsuccess = iterateCursor;
    request.onerror = unexpectedErrorCallback;
}

function iterateCursor()
{
    debug("iterateCursor():");
    evalAndLog("event.target.transaction.oncomplete = checkCursorResultsAndSetupMutatingCursor;");
    cursor = evalAndLog("cursor = event.target.result;");
    if (cursor) {
        if (cursor.value.name == objectStoreData[0].name) {
            sawRemoved = evalAndLog("sawRemoved = true;");
        }
        if (cursor.value.name == objectStoreData[objectStoreData.length - 1].name) {
            sawAdded = evalAndLog("sawAdded = true;");
        }
        evalAndLog("count++;");
        evalAndLog("cursor.continue();");
    }
}

function checkCursorResultsAndSetupMutatingCursor()
{
    debug("checkCursorResultsAndSetupMutatingCursor():");
    shouldBe("count", "objectStoreData.length - 1");
    shouldBe("sawAdded", "false");
    shouldBe("sawRemoved", "true");

    count = evalAndLog("count = 0;");
    sawAdded = evalAndLog("sawAdded = false;");
    sawRemoved = evalAndLog("sawRemoved = false;");

    request = evalAndLog("request = db.transaction('foo', 'readwrite').objectStore('foo').openCursor();");
    request.onsuccess = iterateMutatingCursor;
    request.onerror = unexpectedErrorCallback;
}

function iterateMutatingCursor()
{
    debug("iterateMutatingCursor():");
    evalAndLog("event.target.transaction.oncomplete = checkMutatingCursorResults;");
    cursor = evalAndLog("cursor = event.target.result;");
    if (cursor) {
        if (cursor.value.name == objectStoreData[0].name) {
            sawRemoved = evalAndLog("sawRemoved = true;");
        }
        if (cursor.value.name == objectStoreData[objectStoreData.length - 1].name) {
            sawAdded = evalAndLog("sawAdded = true;");
        }
        shouldBe("cursor.value.name", "'" + objectStoreData[count].name + "'");
        evalAndLog("count++");

        if (count == 1) {
            objectStore = evalAndLog("objectStore = event.target.transaction.objectStore('foo');");
            request = evalAndLog("request = objectStore.delete(objectStoreData[0].ss);");
            request.onsuccess = addFinalData;
            request.onerror = unexpectedErrorCallback;
        } else {
            cursor.continue();
        }
    }
}

function addFinalData()
{
    debug("addFinalData():");
    request = evalAndLog("request = objectStore.add(objectStoreData[objectStoreData.length - 1]);");
    request.onsuccess = function () { cursor.continue(); }
    request.onerror = unexpectedErrorCallback;
}

function checkMutatingCursorResults()
{
    debug("checkMutatingCursorResults():");
    shouldBe("count", "objectStoreData.length");
    shouldBe("sawAdded", "true");
    shouldBe("sawRemoved", "true");
    finishJSTest();
}
