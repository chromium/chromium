if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB keys ordering and readback from cursors.");

indexedDBTest(prepareDatabase, populateStore);
function prepareDatabase()
{
    db = event.target.result;
    evalAndLog("db.createObjectStore('store')");
}

self.keys = [
    "-Infinity",
    "-Number.MAX_VALUE",
    "-1",
    "-Number.MIN_VALUE",
    "0",
    "Number.MIN_VALUE",
    "1",
    "Number.MAX_VALUE",
    "Infinity",

    "new Date(0)",
    "new Date(1000)",
    "new Date(1317399931023)",

    "''",
    "'\x00'",
    "'a'",
    "'aa'",
    "'b'",
    "'ba'",

    "'\xA2'", // U+00A2 CENT SIGN
    "'\u6C34'", // U+6C34 CJK UNIFIED IDEOGRAPH (water)
    "'\uD834\uDD1E'", // U+1D11E MUSICAL SYMBOL G-CLEF (UTF-16 surrogate pair)
    "'\uFFFD'", // U+FFFD REPLACEMENT CHARACTER

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

    "[new Date(0)]",
    "[new Date(1000)]",
    "[new Date(1317399931023)]",

    "['']",
    "['\x00']",
    "['a']",
    "['aa']",
    "['b']",
    "['ba']",

    "['\xA2']", // U+00A2 CENT SIGN
    "['\u6C34']", // U+6C34 CJK UNIFIED IDEOGRAPH (water)
    "['\uD834\uDD1E']", // U+1D11E MUSICAL SYMBOL G-CLEF (UTF-16 surrogate pair)
    "['\uFFFD']", // U+FFFD REPLACEMENT CHARACTER

    "[[]]",

    "[[], []]",
    "[[], [], []]",

    "[[[]]]",
    "[[[[]]]]"
];

function compare(a, b)
{
    if (typeof a !== typeof b) {
        return false;
    }

    switch (typeof a) {
        case 'undefined': // Not valid keys, but compare anyway.
        case 'boolean': // Not valid keys, but compare anyway.
        case 'string':
            return a === b;
        case 'number':
            if (a === b) {
                return (a !== 0) || (1 / a === 1 / b); // 0 isn't -0
            } else {
                return a !== a && b !== b; // NaN is NaN
            }
        case 'object':
            if (a instanceof Date && b instanceof Date) {
                return +a === +b;
            } else if (a instanceof Array && b instanceof Array) {
                if (a.length !== b.length) {
                    return false;
                }
                for (var i = 0; i < a.length; i++) {
                    if (!compare(a[i], b[i])) {
                        return false;
                    }
                }
                return true;
            }
            // For the purposes of this test, other objects don't count
            return false;
        default:
            return false;
    }
}

function populateStore()
{
    debug("");
    debug("populating store...");
    evalAndLog("trans = db.transaction('store', 'readwrite', {durability: 'relaxed'})");
    evalAndLog("store = trans.objectStore('store');");
    trans.onerror = unexpectedErrorCallback;
    trans.onabort = unexpectedAbortCallback;
    var count = 0;
    keys.forEach(function(key) {
        evalAndLog("store.put(" + (count++) + ", " + key + ")");
    });
    trans.oncomplete = checkStore;
}

function checkStore()
{
    debug("");
    debug("iterating cursor...");
    evalAndLog("trans = db.transaction('store', 'readonly', {durability: 'relaxed'})");
    evalAndLog("store = trans.objectStore('store');");
    trans.onerror = unexpectedErrorCallback;
    trans.onabort = unexpectedAbortCallback;
    evalAndLog("count = 0");
    curreq = evalAndLog("curreq = store.openCursor()");
    curreq.onerror = unexpectedErrorCallback;
    curreq.onsuccess = function() {
        if (curreq.result) {
            evalAndLog("cursor = curreq.result");
            shouldBeTrue("compare(cursor.key, " + keys[count] + ")");
            evalAndLog("getreq = store.get(cursor.key)");
            getreq.onerror = unexpectedErrorCallback;
            getreq.onsuccess = function() {
                shouldBe("getreq.result", "count++");
                cursor.continue();
            };
        } else {
            shouldBe("count", "keys.length");
            finishUp();
        }
    };
}

function finishUp()
{
    testKeyCompare();
    finishJSTest();
}

function testKeyCompare()
{
    debug("");
    debug("validate compare function");
    var cases = [
        "undefined",
        "true",
        "false",
        "0",
        "-0",
        "123",
        "Infinity",
        "-Infinity",
        "NaN",
        "''",
        "'abc'",
        "'xyz'",
        "new Date(0)",
        "new Date(1e3)",
        "new Date(1e9)",
        "[]",
        "[123]",
        "['abc']",
        "[123, 'abc']",
        "['abc', 123]",
        "[[]]",
        "[[123]]",
        "[['abc']]",
        "[[123], 'abc']",
        "[[123], 123]"
    ];

    for (var i = 0; i < cases.length; ++i) {
        for (var j = 0; j < cases.length; ++j) {
            if (i === j) {
                shouldBeTrue("compare(" + cases[i] + ", " + cases[j] + ")");
            } else {
                shouldBeFalse("compare(" + cases[i] + ", " + cases[j] + ")");
            }
        }
    }
}
