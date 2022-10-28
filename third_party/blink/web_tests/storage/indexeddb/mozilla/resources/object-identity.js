// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_object_identity.html?force=1
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB: object identity");

indexedDBTest(prepareDatabase, testIdentitySomeMore);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    transaction = evalAndLog("transaction = event.target.transaction;");
    objectStore1 = evalAndLog("objectStore1 = db.createObjectStore('foo');");
    objectStore2 = evalAndLog("objectStore2 = transaction.objectStore('foo');");
    shouldBeTrue("objectStore1 === objectStore2");
    index1 = evalAndLog("index1 = objectStore1.createIndex('bar', 'key');");
    index2 = evalAndLog("index2 = objectStore2.index('bar');");
    shouldBeTrue("index1 === index2");
}

function testIdentitySomeMore()
{
    transaction = evalAndLog("transaction = db.transaction('foo', 'readonly', {durability: 'relaxed'});");
    objectStore3 = evalAndLog("objectStore3 = transaction.objectStore('foo');");
    evalAndLog("objectStore3.someProperty = 'xyz'");
    objectStore4 = evalAndLog("objectStore4 = transaction.objectStore('foo');");
    shouldBeTrue("objectStore3 === objectStore4");
    shouldBeEqualToString("objectStore4.someProperty", "xyz");

    shouldBeFalse("objectStore3 === objectStore1");
    shouldBeFalse("objectStore4 === objectStore2");

    index3 = evalAndLog("index3 = objectStore3.index('bar');");
    index4 = evalAndLog("index4 = objectStore4.index('bar');");
    shouldBeTrue("index3 === index4");

    shouldBeFalse("index3 === index1");
    shouldBeFalse("index4 === index2");

    finishJSTest();
}
