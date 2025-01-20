if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB key types");

indexedDBTest(prepareDatabase, testValidArrayKeys);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    objectStore = evalAndLog("db.createObjectStore('store');");
    debug("");
}

function testValidArrayKeys()
{
    evalAndLog("trans = db.transaction('store', 'readwrite')");
    evalAndLog("store = trans.objectStore('store')");
    debug("");

    evalAndLog("long_array = []; for (i = 0; i < 1000; ++i) { long_array.push('abc', 123, new Date(0), []); }");
    debug("");

    debug("array that contains non-numeric self-reference");
    evalAndLog("self_referrential_array = []; self_referrential_array.self = self_referrential_array;");
    debug("");

    var n = 0, cases = [
        "[]",

        "[-Infinity]",
        "[-Number.MAX_VALUE]",
        "[-1]",
        "[-Number.MIN_VALUE]",
        "[0]",
        "[Number.MIN_VALUE]",
        "[1]",
        "[Number.MAX_VALUE]",
        "[Infinity]",

        "[1,2,3]",

        "[new Date(0)]",
        "[new Date('2525-01-01T00:00:00Z')]",

        "[new Date(0), new Date('2525-01-01T00:00:00Z')]",

        "['']",
        "['\x00']",
        "['abc123']",

        "['abc', 123]",

        "[[]]",

        "[[], []]",
        "[[], [], []]",

        "[[[]]]",
        "[[[[]]]]",

        "[123, 'abc', new Date(0), []]",
        "[[123, 'abc', new Date(0), []], [456, 'def', new Date(999), [[]]]]",

        "long_array",
        "self_referrential_array"
    ];

    function testArrayPutGet(value, key, callback)
    {
        debug("testing array key: " + key);
        putreq = evalAndLog("store.put('" + value + "', " + key + ");");
        putreq.onerror = unexpectedErrorCallback;
        putreq.onsuccess = function() {
            getreq = evalAndLog("store.get(" + key + ");");
            getreq.onerror = unexpectedErrorCallback;
            getreq.onsuccess = function() {
                shouldBeEqualToString("getreq.result", value);
                debug("");
                callback();
            };
        };
    }

    function nextTest()
    {
        var testcase = cases.shift();
        if (testcase) {
            testArrayPutGet("value" + (++n), testcase, nextTest);
        }
    }

    nextTest();

    trans.oncomplete = testInvalidArrayKeys;
}

function testInvalidArrayKeys()
{
    evalAndLog("trans = db.transaction('store', 'readwrite')");
    evalAndLog("store = trans.objectStore('store')");
    debug("");

    debug("array that contains itself: array = [ array ]");
    evalAndLog("cyclic_array = []; cyclic_array.push(cyclic_array)");

    debug("array that contains itself, one level down: array = [ [ array ] ]");
    evalAndLog("cyclic_array2 = []; cyclic_array2.push([cyclic_array2])");

    debug("array that contains itself, not as first element: array = [1, 'b', [], array]");
    evalAndLog("cyclic_array3 = [1, 'b', []]; cyclic_array3.push(cyclic_array3)");

    debug("array that contains array that contains itself");
    evalAndLog("cyclic_array4 = [cyclic_array];");
    debug("");

    var invalidKeys = [
        "[ void 0 ]", // undefined
        "[ true ]",
        "[ false ]",
        "[ NaN ]",
        "[ null ]",
        "[ {} ]",
        "[ function () {} ]",
        "[ /regex/ ]",
        "[ self ]",
        "[ self.document ]",
        "[ self.document.body ]",
        "cyclic_array",
        "cyclic_array2",
        "cyclic_array3",
        "cyclic_array4",
        "Array(1000)" // sparse
    ];

    invalidKeys.forEach(function (key) {
        debug("testing invalid array key: " + key);
        evalAndExpectException("store.put('value', " + key + ");", "0", "'DataError'");
        debug("");
    });

    testDepthLimits();
}

function makeArrayOfDepth(n)
{
    var array = [];
    while (--n) {
        array = [array];
    }
    return array;
}

function testDepthLimits()
{
    shouldBe("indexedDB.cmp(makeArrayOfDepth(25), 0)", "1");
    shouldBe("indexedDB.cmp(makeArrayOfDepth(250), 0)", "1");
    evalAndExpectException("indexedDB.cmp(makeArrayOfDepth(2500), 0)", "0", "'DataError'");
    debug("");

    finishJSTest();
}
