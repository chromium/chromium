/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.jsonTest');
goog.setTestOnly();

const functions = goog.require('goog.functions');
const googJson = goog.require('goog.json');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

function allChars(start, end) {
  const sb = [];
  for (let i = start; i < end; i++) {
    sb.push(String.fromCharCode(i));
  }
  return sb.join('');
}

// serialization

// parsing

/**
 * Asserts that the given object serializes to the given value.
 * If the current browser has an implementation of JSON.serialize,
 * we make sure our version matches that one.
 */
function assertSerialize(expected, obj, replacer = undefined) {
  assertEquals(expected, googJson.serialize(obj, replacer));

  // goog.json.serialize escapes non-ASCI characters while JSON.stringify
  // doesn’t.  This is expected so do not compare the results.
  if (typeof obj == 'string' && obj.charCodeAt(0) > 0x7f) return;

  // I'm pretty sure that the goog.json.serialize behavior is correct by the ES5
  // spec, but JSON.stringify(undefined) is undefined on all browsers.
  if (obj === undefined) return;

  // Browsers don't serialize undefined properties, but goog.json.serialize does
  if (goog.isObject(obj) && ('a' in obj) && obj['a'] === undefined) return;

  // Replacers are broken on IE and older versions of firefox.
  if (replacer && !userAgent.WEBKIT) return;

  // goog.json.serialize does not stringify dates the same way.
  if (obj instanceof Date) return;

  // goog.json.serialize does not stringify functions the same way.
  if (obj instanceof Function) return;

  // goog.json.serialize doesn't use the toJSON method.
  if (goog.isObject(obj) && typeof obj.toJSON === 'function') return;

  if (typeof JSON != 'undefined') {
    assertEquals(
        'goog.json.serialize does not match JSON.stringify', expected,
        JSON.stringify(obj, replacer));
  }
}

/**
 * @param {string} a
 * @param {string} b
 * @return {string} any common character between two strings a and b.
 */
function findCommonChar(a, b) {
  for (let i = 0; i < b.length; i++) {
    if (a.indexOf(b.charAt(i)) >= 0) {
      return b.charAt(i);
    }
  }
  return '';
}
testSuite({
  testStringSerialize() {
    assertSerialize('""', '');

    assertSerialize('"true"', 'true');
    assertSerialize('"false"', 'false');
    assertSerialize('"null"', 'null');
    assertSerialize('"0"', '0');

    // Unicode and control characters
    assertSerialize('"\\n"', '\n');
    assertSerialize('"\\u001f"', '\x1f');
    assertSerialize('"\\u20ac"', '\u20AC');
    assertSerialize('"\\ud83d\\ud83d"', '\ud83d\ud83d');

    const str = allChars(0, 10000);
    assertEquals(str, eval(googJson.serialize(str)));
  },

  testNullSerialize() {
    assertSerialize('null', null);
    assertSerialize('null', undefined);
    assertSerialize('null', NaN);

    assertSerialize('0', 0);
    assertSerialize('""', '');
    assertSerialize('false', false);
  },

  testNullPropertySerialize() {
    assertSerialize('{"a":null}', {'a': null});
    assertSerialize('{"a":null}', {'a': undefined});
  },

  testNumberSerialize() {
    assertSerialize('0', 0);
    assertSerialize('12345', 12345);
    assertSerialize('-12345', -12345);

    assertSerialize('0.1', 0.1);
    // the leading zero may not be omitted
    assertSerialize('0.1', .1);

    // no leading +
    assertSerialize('1', +1);

    // either format is OK
    let s = googJson.serialize(1e50);
    assertTrue(
        '1e50', s == '1e50' || s == '1E50' || s == '1e+50' || s == '1E+50');

    // either format is OK
    s = googJson.serialize(1e-50);
    assertTrue('1e50', s == '1e-50' || s == '1E-50');

    // These numbers cannot be represented in JSON
    assertSerialize('null', NaN);
    assertSerialize('null', Infinity);
    assertSerialize('null', -Infinity);
  },

  testBooleanSerialize() {
    assertSerialize('true', true);
    assertSerialize('"true"', 'true');

    assertSerialize('false', false);
    assertSerialize('"false"', 'false');
  },

  testArraySerialize() {
    assertSerialize('[]', []);
    assertSerialize('[1]', [1]);
    assertSerialize('[1,2]', [1, 2]);
    assertSerialize('[1,2,3]', [1, 2, 3]);
    assertSerialize('[[]]', [[]]);
    assertSerialize('[null,null]', [() => {}, () => {}]);

    assertNotEquals('{length:0}', googJson.serialize({length: 0}), '[]');
  },

  testFunctionSerialize() {
    assertSerialize('null', () => {});
  },

  testObjectSerialize_emptyObject() {
    assertSerialize('{}', {});
  },

  testObjectSerialize_oneItem() {
    assertSerialize('{"a":"b"}', {a: 'b'});
  },

  testObjectSerialize_twoItems() {
    assertEquals(
        '{"a":"b","c":"d"}', googJson.serialize({a: 'b', c: 'd'}),
        '{"a":"b","c":"d"}');
  },

  testObjectSerialize_whitespace() {
    assertSerialize('{" ":" "}', {' ': ' '});
  },

  testSerializeSkipFunction() {
    const object = {
      s: 'string value',
      b: true,
      i: 100,
      f: function() {
        const x = 'x';
      }
    };
    assertSerialize('null', object.f);
    assertSerialize('{"s":"string value","b":true,"i":100}', object);
  },

  testObjectSerialize_array() {
    assertNotEquals('[0,1]', googJson.serialize([0, 1]), '{"0":"0","1":"1"}');
  },

  testObjectSerialize_recursion() {
    if (userAgent.WEBKIT) {
      return;  // this makes safari 4 crash.
    }

    const anObject = {};
    anObject.thisObject = anObject;
    assertThrows('expected recursion exception', () => {
      googJson.serialize(anObject);
    });
  },

  testObjectSerializeWithHasOwnProperty() {
    const object = {'hasOwnProperty': null};
    assertEquals('{"hasOwnProperty":null}', googJson.serialize(object));
  },

  testWrappedObjects() {
    assertSerialize('"foo"', new String('foo'));
    assertSerialize('42', new Number(42));
    assertSerialize('null', new Number('a NaN'));
    assertSerialize('true', new Boolean(true));
  },

  testStringParse() {
    assertEquals('Empty string', googJson.parse('""'), '');
    assertEquals('whitespace string', googJson.parse('" "'), ' ');

    // unicode without the control characters 0x00 - 0x1f, 0x7f - 0x9f
    const str = allChars(32, 1000);
    const jsonString = googJson.serialize(str);
    const a = eval(jsonString);
    assertEquals('unicode string', googJson.parse(jsonString), a);

    assertEquals('true as a string', googJson.parse('"true"'), 'true');
    assertEquals('false as a string', googJson.parse('"false"'), 'false');
    assertEquals('null as a string', googJson.parse('"null"'), 'null');
    assertEquals('number as a string', googJson.parse('"0"'), '0');
  },

  testNullParse() {
    assertEquals('null', googJson.parse(null), null);
    assertEquals('null', googJson.parse('null'), null);

    assertNotEquals('0', googJson.parse('0'), null);
    assertNotEquals('""', googJson.parse('""'), null);
    assertNotEquals('false', googJson.parse('false'), null);
  },

  testNumberParse() {
    assertEquals('0', googJson.parse('0'), 0);
    assertEquals('12345', googJson.parse('12345'), 12345);
    assertEquals('-12345', googJson.parse('-12345'), -12345);

    assertEquals('0.1', googJson.parse('0.1'), 0.1);

    // either format is OK
    assertEquals(1e50, googJson.parse('1e50'));
    assertEquals(1e50, googJson.parse('1E50'));
    assertEquals(1e50, googJson.parse('1e+50'));
    assertEquals(1e50, googJson.parse('1E+50'));

    // either format is OK
    assertEquals(1e-50, googJson.parse('1e-50'));
    assertEquals(1e-50, googJson.parse('1E-50'));
  },

  testBooleanParse() {
    assertEquals('true', googJson.parse('true'), true);
    assertEquals('false', googJson.parse('false'), false);

    assertNotEquals('0', googJson.parse('0'), false);
    assertNotEquals('"false"', googJson.parse('"false"'), false);
    assertNotEquals('null', googJson.parse('null'), false);

    assertNotEquals('1', googJson.parse('1'), true);
    assertNotEquals('"true"', googJson.parse('"true"'), true);
    assertNotEquals('{}', googJson.parse('{}'), true);
    assertNotEquals('[]', googJson.parse('[]'), true);
  },

  testArrayParse() {
    assertArrayEquals([], googJson.parse('[]'));
    assertArrayEquals([1], googJson.parse('[1]'));
    assertArrayEquals([1, 2], googJson.parse('[1,2]'));
    assertArrayEquals([1, 2, 3], googJson.parse('[1,2,3]'));
    assertArrayEquals([[]], googJson.parse('[[]]'));

    // Note that array-holes are not valid json. However, goog.json.parse
    // supports them so that clients can reap the security benefits of
    // goog.json.parse even if they are using this non-standard format.
    assertArrayEquals([1, /* hole */, 3], googJson.parse('[1,,3]'));

    // make sure we do not get an array for something that looks like an array
    assertFalse('{length:0}', 'push' in googJson.parse('{"length":0}'));
  },

  testObjectParse() {
    function objectEquals(a1, a2) {
      for (let key in a1) {
        if (a1[key] != a2[key]) {
          return false;
        }
      }
      return true;
    }

    assertTrue('{}', objectEquals(googJson.parse('{}'), {}));
    assertTrue(
        '{"a":"b"}', objectEquals(googJson.parse('{"a":"b"}'), {a: 'b'}));
    assertTrue(
        '{"a":"b","c":"d"}',
        objectEquals(googJson.parse('{"a":"b","c":"d"}'), {a: 'b', c: 'd'}));
    assertTrue(
        '{" ":" "}', objectEquals(googJson.parse('{" ":" "}'), {' ': ' '}));

    // make sure we do not get an Object when it is really an array
    assertTrue('[0,1]', 'length' in googJson.parse('[0,1]'));
  },

  testForValidJson() {
    function error_(msg, s) {
      assertThrows(
          `${msg}, Should have raised an exception: ${s}`,
          goog.partial(googJson.parse, s));
    }

    error_('Non closed string', '"dasdas');
    error_('undefined is not valid json', 'undefined');

    // These numbers cannot be represented in JSON
    error_('NaN cannot be presented in JSON', 'NaN');
    error_('Infinity cannot be presented in JSON', 'Infinity');
    error_('-Infinity cannot be presented in JSON', '-Infinity');
  },

  testIsNotValid() {
    assertFalse(googJson.isValid('t'));
    assertFalse(googJson.isValid('r'));
    assertFalse(googJson.isValid('u'));
    assertFalse(googJson.isValid('e'));
    assertFalse(googJson.isValid('f'));
    assertFalse(googJson.isValid('a'));
    assertFalse(googJson.isValid('l'));
    assertFalse(googJson.isValid('s'));
    assertFalse(googJson.isValid('n'));
    assertFalse(googJson.isValid('E'));

    assertFalse(googJson.isValid('+'));
    assertFalse(googJson.isValid('-'));

    assertFalse(googJson.isValid('t++'));
    assertFalse(googJson.isValid('++t'));
    assertFalse(googJson.isValid('t--'));
    assertFalse(googJson.isValid('--t'));
    assertFalse(googJson.isValid('-t'));
    assertFalse(googJson.isValid('+t'));

    assertFalse(googJson.isValid('"\\"'));  // "\"
    assertFalse(googJson.isValid('"\\'));   // "\

    // multiline string using \ at the end is not valid
    assertFalse(googJson.isValid('"a\\\nb"'));

    assertFalse(googJson.isValid('"\n"'));
    assertFalse(googJson.isValid('"\r"'));
    assertFalse(googJson.isValid('"\r\n"'));
    // Disallow the unicode newlines
    assertFalse(googJson.isValid('"\u2028"'));
    assertFalse(googJson.isValid('"\u2029"'));

    assertFalse(googJson.isValid(' '));
    assertFalse(googJson.isValid('\n'));
    assertFalse(googJson.isValid('\r'));
    assertFalse(googJson.isValid('\r\n'));

    assertFalse(googJson.isValid('t.r'));

    assertFalse(googJson.isValid('1e'));
    assertFalse(googJson.isValid('1e-'));
    assertFalse(googJson.isValid('1e+'));

    assertFalse(googJson.isValid('1e-'));

    assertFalse(googJson.isValid('"\\\u200D\\"'));
    assertFalse(googJson.isValid('"\\\0\\"'));
    assertFalse(googJson.isValid('"\\\0"'));
    assertFalse(googJson.isValid('"\\0"'));
    assertFalse(googJson.isValid('"\x0c"'));

    assertFalse(googJson.isValid('"\\\u200D\\", alert(\'foo\') //"\n'));

    // Disallow referencing variables with names built up from primitives
    assertFalse(googJson.isValid('truefalse'));
    assertFalse(googJson.isValid('null0'));
    assertFalse(googJson.isValid('null0.null0'));
    assertFalse(googJson.isValid('[truefalse]'));
    assertFalse(googJson.isValid('{"a": null0}'));
    assertFalse(googJson.isValid('{"a": null0, "b": 1}'));
  },

  testIsValid() {
    assertTrue(googJson.isValid('\n""\n'));
    assertTrue(googJson.isValid('[1\n,2\r,3\u2028\n,4\u2029]'));
    assertTrue(googJson.isValid('"\x7f"'));
    assertTrue(googJson.isValid('"\x09"'));
    // Test tab characters in json.
    assertTrue(googJson.isValid('{"\t":"\t"}'));
  },

  testDoNotSerializeProto() {
    function F() {}
    F.prototype = {c: 3};

    /** @suppress {checkTypes} suppression added to enable type checking */
    const obj = new F;
    obj.a = 1;
    obj.b = 2;

    assertEquals(
        'Should not follow the prototype chain', '{"a":1,"b":2}',
        googJson.serialize(obj));
  },

  testEscape() {
    const unescaped = '1a*/]';
    assertEquals(
        'Should not escape', `"${unescaped}"`, googJson.serialize(unescaped));

    const escaped = '\n\x7f\u1049';
    assertEquals(
        'Should escape', '',
        findCommonChar(escaped, googJson.serialize(escaped)));
    assertEquals(
        'Should eval to the same string after escaping', escaped,
        googJson.parse(googJson.serialize(escaped)));
  },

  testReplacer() {
    assertSerialize('[null,null,0]', [, , 0]);

    assertSerialize('[0,0,{"x":0}]', [, , {x: 0}], function(k, v) {
      if (v === undefined && Array.isArray(this)) {
        return 0;
      }
      return v;
    });

    assertSerialize('[0,1,2,3]', [0, 0, 0, 0], (k, v) => {
      const kNum = Number(k);
      if (k && !isNaN(kNum)) {
        return kNum;
      }
      return v;
    });

    const f = (k, v) => typeof v == 'number' ? v + 1 : v;
    assertSerialize('{"a":1,"b":{"c":2}}', {'a': 0, 'b': {'c': 1}}, f);
  },

  testDateSerialize() {
    assertSerialize('{}', new Date(0));
  },

  testToJSONSerialize() {
    assertSerialize('{}', {toJSON: functions.constant('serialized')});
    assertSerialize('{"toJSON":"normal"}', {toJSON: 'normal'});
  },

  testTryNativeJson() {
    // bypass the compiler @define check
    googJson['TRY_NATIVE_JSON'] = true;
    let error;
    googJson.setErrorLogger((message, ex) => {
      error = message;
    });

    error = undefined;
    googJson.parse('{"a":[,1]}');
    assertEquals('Invalid JSON: {"a":[,1]}', error);

    // bypass the compiler @define check
    googJson['TRY_NATIVE_JSON'] = false;
    googJson.setErrorLogger(goog.nullFunction);
  },
});
