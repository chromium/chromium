/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.objectTest');
goog.setTestOnly();

const functions = goog.require('goog.functions');
const googArray = goog.require('goog.array');
const googObject = goog.require('goog.object');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');
const {assertInstanceof} = goog.require('goog.asserts');

function stringifyObject(m) {
  const keys = googObject.getKeys(m);
  let s = '';
  for (let i = 0; i < keys.length; i++) {
    s += keys[i] + googObject.get(m, keys[i]);
  }
  return s;
}

function getObject() {
  return {a: 0, b: 1, c: 2, d: 3};
}

function createRecordedGetFoo() {
  return recordFunction(functions.constant('foo'));
}

function createTestDeepObject() {
  const obj = {};
  obj.a = {};
  obj.a.b = {};
  obj.a.b.c = {};
  obj.a.b.c.fooArr = [5, 6, 7, 8];
  obj.a.b.c.knownNull = null;
  obj.knownEmptyString = '';
  obj.knownSingleCharacterString = '1';

  return obj;
}

testSuite({
  testKeys() {
    const m = getObject();
    assertEquals(
        'getKeys, The keys should be a,b,c', 'a,b,c,d',
        googObject.getKeys(m).join(','));
  },

  testValues() {
    const m = getObject();
    assertEquals(
        'getValues, The values should be 0,1,2', '0,1,2,3',
        googObject.getValues(m).join(','));
  },

  testGetAnyKey() {
    const m = getObject();
    assertTrue(
        'getAnyKey, The key should be a,b,c or d',
        googObject.getAnyKey(m) in m);
    assertUndefined(
        'getAnyKey, The key should be undefined', googObject.getAnyKey({}));
  },

  testGetAnyValue() {
    const m = getObject();
    assertTrue(
        'getAnyValue, The value should be 0,1,2 or 3',
        googObject.containsValue(m, googObject.getAnyValue(m)));
    assertUndefined(
        'getAnyValue, The value should be undefined',
        googObject.getAnyValue({}));
  },

  testContainsKey() {
    const m = getObject();
    assertTrue(
        'containsKey, Should contain the \'a\' key',
        googObject.containsKey(m, 'a'));
    assertFalse(
        'containsKey, Should not contain the \'e\' key',
        googObject.containsKey(m, 'e'));
  },

  testContainsValue() {
    const m = getObject();
    assertTrue(
        'containsValue, Should contain the value 0',
        googObject.containsValue(m, 0));
    assertFalse(
        'containsValue, Should not contain the value 4',
        googObject.containsValue(m, 4));
    assertTrue('isEmpty, The map should not be empty', !googObject.isEmpty(m));
  },

  testFindKey() {
    const dict = {'a': 1, 'b': 2, 'c': 3, 'd': 4};
    const key = googObject.findKey(dict, (v, k, d) => {
      assertEquals('valid 3rd argument', dict, d);
      assertTrue('valid 1st argument', googObject.containsValue(d, v));
      assertTrue('valid 2nd argument', k in d);
      return v % 3 == 0;
    });
    assertEquals('key "c" found', 'c', key);

    const pred = (value) => value > 5;
    assertUndefined('no match', googObject.findKey(dict, pred));
  },

  testFindValue() {
    const dict = {'a': 1, 'b': 2, 'c': 3, 'd': 4};
    const value = googObject.findValue(dict, (v, k, d) => {
      assertEquals('valid 3rd argument', dict, d);
      assertTrue('valid 1st argument', googObject.containsValue(d, v));
      assertTrue('valid 2nd argument', k in d);
      return k.toUpperCase() == 'C';
    });
    assertEquals('value 3 found', 3, value);

    const pred = (value, key) => key > 'd';
    assertUndefined('no match', googObject.findValue(dict, pred));
  },

  testClear() {
    const m = getObject();
    googObject.clear(m);
    assertTrue('cleared so it should be empty', googObject.isEmpty(m));
    assertFalse(
        'cleared so it should not contain \'a\' key',
        googObject.containsKey(m, 'a'));
  },

  testClone() {
    const m = getObject();
    const m2 = googObject.clone(m);
    assertFalse('clone so it should not be empty', googObject.isEmpty(m2));
    assertTrue(
        'clone so it should contain \'c\' key',
        googObject.containsKey(m2, 'c'));
  },

  testUnsafeClonePrimitive() {
    assertEquals(
        'cloning a primitive should return an equal primitive', 5,
        googObject.unsafeClone(5));
  },

  testUnsafeCloneObjectThatHasACloneMethod() {
    const original = {
      name: 'original',
      clone: functions.constant({name: 'clone'}),
    };

    const clone = googObject.unsafeClone(original);
    assertEquals('original', original.name);
    assertEquals('clone', clone.name);
  },

  testUnsafeCloneObjectThatHasACloneNonMethod() {
    const originalIndex = {red: [0, 4], clone: [1, 3, 5, 7], yellow: [2, 6]};

    const clone = googObject.unsafeClone(originalIndex);
    assertArrayEquals([1, 3, 5, 7], originalIndex.clone);
    assertArrayEquals([1, 3, 5, 7], clone.clone);
  },

  testUnsafeCloneFlatObject() {
    const original = {a: 1, b: 2, c: 3};
    const clone = googObject.unsafeClone(original);
    assertNotEquals(original, clone);
    assertObjectEquals(original, clone);
  },

  testUnsafeCloneDeepObject() {
    const original = {a: 1, b: {c: 2, d: 3}, e: {f: {g: 4, h: 5}}};
    const clone = googObject.unsafeClone(original);

    assertNotEquals(original, clone);
    assertNotEquals(original.b, clone.b);
    assertNotEquals(original.e, clone.e);

    assertEquals(1, clone.a);
    assertEquals(2, clone.b.c);
    assertEquals(3, clone.b.d);
    assertEquals(4, clone.e.f.g);
    assertEquals(5, clone.e.f.h);
  },

  testUnsafeCloneMapWithDeepObject() {
    const original =
        new Map([['a', 1], ['b', {c: 2, d: 3}], ['e', {f: {g: 4, h: 5}}]]);
    const clone = googObject.unsafeClone(original);

    assertInstanceof(clone, Map);
    assertNotEquals(original, clone);
    // Shallow clone, not deep
    assertEquals(original.get('b'), clone.get('b'));
    assertEquals(original.get('e'), clone.get('e'));
    assertEquals(original.get('e').f, clone.get('e').f);
    assertEquals(1, clone.get('a'));
    assertEquals(2, clone.get('b').c);
    assertEquals(3, clone.get('b').d);
    assertEquals(4, clone.get('e').f.g);
    assertEquals(5, clone.get('e').f.h);
  },

  testUnsafeCloneSetWithDeepObject() {
    const container1 = {c: 2, d: 3};
    const container2 = {f: {g: 4, h: 5}};
    const original = new Set([container1, container2]);
    const clone = googObject.unsafeClone(original);

    assertInstanceof(clone, Set);
    assertNotEquals(original, clone);
    // Shallow clone, not deep.
    assertTrue(clone.has(container1));
    assertTrue(clone.has(container2));
    const newSetValues = Array.from(clone.values());
    assertEquals(container2.f, newSetValues[1].f);
    assertEquals(2, newSetValues[0].c);
    assertEquals(3, newSetValues[0].d);
    assertEquals(4, newSetValues[1].f.g);
    assertEquals(5, newSetValues[1].f.h);
  },

  testUnsafeCloneTypedArray() {
    if (typeof ArrayBuffer !== 'function' ||
        typeof ArrayBuffer.isView !== 'function') {
      return;
    }
    function array(ctor, ...elems) {  // IE 11 does not support TypedArray.of
      const arr = new ctor(elems.length);
      for (let i = 0; i < elems.length; i++) {
        arr[i] = elems[i];
      }
      return arr;
    }

    const original = {a: array(Uint8Array, 1, 2), b: array(Int16Array, 3, 4)};
    const clone = googObject.unsafeClone(original);

    assertNotEquals(original, clone);
    assertNotEquals(original.a, clone.a);
    assertNotEquals(original.b, clone.b);

    assertTrue(clone.a instanceof Uint8Array);
    assertEquals(2, clone.a.length);
    assertEquals(1, clone.a[0]);
    assertEquals(2, clone.a[1]);
    assertTrue(clone.b instanceof Int16Array);
    assertEquals(2, clone.b.length);
    assertEquals(3, clone.b[0]);
    assertEquals(4, clone.b[1]);
  },

  testUnsafeCloneFunctions() {
    const original = {f: functions.constant('hi')};
    const clone = googObject.unsafeClone(original);

    assertNotEquals(original, clone);
    assertEquals('hi', clone.f());
    assertEquals(original.f, clone.f);
  },

  testForEach() {
    const m = getObject();
    let s = '';
    googObject.forEach(m, (val, key, m2) => {
      assertNotUndefined(key);
      assertEquals(m, m2);
      s += key + val;
    });
    assertEquals(s, 'a0b1c2d3');
  },

  testFilter() {
    const m = getObject();

    const m2 = googObject.filter(m, (val, key, m3) => {
      assertNotUndefined(key);
      assertEquals(m, m3);
      return val > 1;
    });
    assertEquals(stringifyObject(m2), 'c2d3');
  },

  testMap() {
    const m = getObject();
    const m2 = googObject.map(m, (val, key, m3) => {
      assertNotUndefined(key);
      assertEquals(m, m3);
      return val * val;
    });
    assertEquals(stringifyObject(m2), 'a0b1c4d9');
  },

  testSome() {
    const m = getObject();
    let b = googObject.some(m, (val, key, m2) => {
      assertNotUndefined(key);
      assertEquals(m, m2);
      return val > 1;
    });
    assertTrue(b);
    b = googObject.some(m, (val, key, m2) => {
      assertNotUndefined(key);
      assertEquals(m, m2);
      return val > 100;
    });
    assertFalse(b);
  },

  testEvery() {
    const m = getObject();
    let b = googObject.every(m, (val, key, m2) => {
      assertNotUndefined(key);
      assertEquals(m, m2);
      return val >= 0;
    });
    assertTrue(b);
    b = googObject.every(m, (val, key, m2) => {
      assertNotUndefined(key);
      assertEquals(m, m2);
      return val > 1;
    });
    assertFalse(b);
  },

  testContains() {
    const m = getObject();
    assertTrue(googObject.contains(m, 3));
    assertFalse(googObject.contains(m, 4));
  },

  testObjectProperties() {
    const m = {};

    googObject.set(m, 'toString', 'once');
    googObject.set(m, 'valueOf', 'upon');
    googObject.set(m, 'eval', 'a');
    googObject.set(m, 'toSource', 'midnight');
    googObject.set(m, 'prototype', 'dreary');
    googObject.set(m, 'hasOwnProperty', 'dark');

    assertEquals(googObject.get(m, 'toString'), 'once');
    assertEquals(googObject.get(m, 'valueOf'), 'upon');
    assertEquals(googObject.get(m, 'eval'), 'a');
    assertEquals(googObject.get(m, 'toSource'), 'midnight');
    assertEquals(googObject.get(m, 'prototype'), 'dreary');
    assertEquals(googObject.get(m, 'hasOwnProperty'), 'dark');
  },

  testSetDefault() {
    const dict = {};
    assertEquals(1, googObject.setIfUndefined(dict, 'a', 1));
    assertEquals(1, dict['a']);
    assertEquals(1, googObject.setIfUndefined(dict, 'a', 2));
    assertEquals(1, dict['a']);
  },

  testSetWithReturnValueNotSet_KeyIsSet() {
    const f = createRecordedGetFoo();
    const obj = {};
    obj['key'] = 'bar';
    assertEquals('bar', googObject.setWithReturnValueIfNotSet(obj, 'key', f));
    f.assertCallCount(0);
  },

  testSetWithReturnValueNotSet_KeyIsNotSet() {
    const f = createRecordedGetFoo();
    const obj = {};
    assertEquals('foo', googObject.setWithReturnValueIfNotSet(obj, 'key', f));
    f.assertCallCount(1);
  },

  testSetWithReturnValueNotSet_KeySetValueIsUndefined() {
    const f = createRecordedGetFoo();
    const obj = {};
    obj['key'] = undefined;
    assertEquals(
        undefined, googObject.setWithReturnValueIfNotSet(obj, 'key', f));
    f.assertCallCount(0);
  },

  testTranspose() {
    const m = getObject();
    const b = googObject.transpose(m);
    assertEquals('a', b[0]);
    assertEquals('b', b[1]);
    assertEquals('c', b[2]);
    assertEquals('d', b[3]);
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testExtend() {
    let o = {};
    let o2 = {a: 0, b: 1};
    googObject.extend(o, o2);
    assertEquals(0, o.a);
    assertEquals(1, o.b);
    assertTrue('a' in o);
    assertTrue('b' in o);

    o2 = {c: 2};
    googObject.extend(o, o2);
    assertEquals(2, o.c);
    assertTrue('c' in o);

    o2 = {c: 3};
    googObject.extend(o, o2);
    assertEquals(3, o.c);
    assertTrue('c' in o);

    o = {};
    o2 = {c: 2};
    let o3 = {c: 3};
    googObject.extend(o, o2, o3);
    assertEquals(3, o.c);
    assertTrue('c' in o);

    o = {};
    o2 = {a: 0, b: 1};
    o3 = {c: 2, d: 3};
    googObject.extend(o, o2, o3);
    assertEquals(0, o.a);
    assertEquals(1, o.b);
    assertEquals(2, o.c);
    assertEquals(3, o.d);
    assertTrue('a' in o);
    assertTrue('b' in o);
    assertTrue('c' in o);
    assertTrue('d' in o);

    o = {};
    o2 = {
      'constructor': 0,
      'hasOwnProperty': 1,
      'isPrototypeOf': 2,
      'propertyIsEnumerable': 3,
      'toLocaleString': 4,
      'toString': 5,
      'valueOf': 6,
    };
    googObject.extend(o, o2);
    assertEquals(0, o['constructor']);
    assertEquals(1, o['hasOwnProperty']);
    assertEquals(2, o['isPrototypeOf']);
    assertEquals(3, o['propertyIsEnumerable']);
    assertEquals(4, o['toLocaleString']);
    assertEquals(5, o['toString']);
    assertEquals(6, o['valueOf']);
    assertTrue('constructor' in o);
    assertTrue('hasOwnProperty' in o);
    assertTrue('isPrototypeOf' in o);
    assertTrue('propertyIsEnumerable' in o);
    assertTrue('toLocaleString' in o);
    assertTrue('toString' in o);
    assertTrue('valueOf' in o);
  },

  testCreate() {
    assertObjectEquals(
        'With multiple arguments', {a: 0, b: 1},
        googObject.create('a', 0, 'b', 1));
    assertObjectEquals(
        'With an array argument', {a: 0, b: 1},
        googObject.create(['a', 0, 'b', 1]));

    assertObjectEquals('With no arguments', {}, googObject.create());
    assertObjectEquals(
        'With an ampty array argument', {}, googObject.create([]));

    assertThrows('Should throw due to uneven arguments', () => {
      googObject.create('a');
    });
    assertThrows('Should throw due to uneven arguments', () => {
      googObject.create('a', 0, 'b');
    });
    assertThrows('Should throw due to uneven length array', () => {
      googObject.create(['a']);
    });
    assertThrows('Should throw due to uneven length array', () => {
      googObject.create(['a', 0, 'b']);
    });
  },

  testCreateSet() {
    assertObjectEquals(
        'With multiple arguments', {a: true, b: true},
        googObject.createSet('a', 'b'));
    assertObjectEquals(
        'With an array argument', {a: true, b: true},
        googObject.createSet(['a', 'b']));

    assertObjectEquals('With no arguments', {}, googObject.createSet());
    assertObjectEquals(
        'With an ampty array argument', {}, googObject.createSet([]));
  },

  testGetValueByKeys() {
    const obj = createTestDeepObject();
    assertEquals(obj, googObject.getValueByKeys(obj));
    assertEquals(obj.a, googObject.getValueByKeys(obj, 'a'));
    assertEquals(obj.a.b, googObject.getValueByKeys(obj, 'a', 'b'));
    assertEquals(obj.a.b.c, googObject.getValueByKeys(obj, 'a', 'b', 'c'));
    assertEquals(
        obj.a.b.c.d, googObject.getValueByKeys(obj, 'a', 'b', 'c', 'd'));
    assertEquals(8, googObject.getValueByKeys(obj, 'a', 'b', 'c', 'fooArr', 3));
    assertNull(googObject.getValueByKeys(obj, 'a', 'b', 'c', 'knownNull'));
    assertUndefined(
        googObject.getValueByKeys(obj, 'a', 'b', 'c', 'knownNull', 'd'));
    assertUndefined(googObject.getValueByKeys(obj, 'e', 'f', 'g'));
    assertEquals(
        0, googObject.getValueByKeys(obj, 'knownEmptyString', 'length'));
    assertEquals(
        1,
        googObject.getValueByKeys(obj, 'knownSingleCharacterString', 'length'));
  },

  testGetValueByKeysArraySyntax() {
    const obj = createTestDeepObject();
    assertEquals(obj, googObject.getValueByKeys(obj, []));
    assertEquals(obj.a, googObject.getValueByKeys(obj, ['a']));

    assertEquals(obj.a.b, googObject.getValueByKeys(obj, ['a', 'b']));
    assertEquals(obj.a.b.c, googObject.getValueByKeys(obj, ['a', 'b', 'c']));
    assertEquals(
        obj.a.b.c.d, googObject.getValueByKeys(obj, ['a', 'b', 'c', 'd']));
    assertEquals(
        8, googObject.getValueByKeys(obj, ['a', 'b', 'c', 'fooArr', 3]));
    assertNull(googObject.getValueByKeys(obj, ['a', 'b', 'c', 'knownNull']));
    assertUndefined(
        googObject.getValueByKeys(obj, ['a', 'b', 'c', 'knownNull', 'd']));
    assertUndefined(googObject.getValueByKeys(obj, 'e', 'f', 'g'));
  },

  testImmutableView() {
    if (!Object.isFrozen) {
      return;
    }
    const x = {propA: 3};
    const y = googObject.createImmutableView(x);
    x.propA = 4;
    x.propB = 6;
    try {
      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      y.propA = 5;
      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      y.propB = 7;
    } catch (e) {
    }
    assertEquals(4, x.propA);
    assertEquals(6, x.propB);
    assertFalse(googObject.isImmutableView(x));

    assertEquals(4, y.propA);
    assertEquals(6, y.propB);
    assertTrue(googObject.isImmutableView(y));

    assertFalse('x and y should be different references', x == y);
    assertTrue(
        'createImmutableView should not create a new view of an immutable object',
        y == googObject.createImmutableView(y));
  },

  testImmutableViewStrict() {
    'use strict';

    // IE9 supports isFrozen, but does not support strict mode. Exit early if we
    // are not actually running in strict mode.
    const isStrict = (function() {
      return !this;
    })();

    if (!Object.isFrozen || !isStrict) {
      return;
    }
    const x = {propA: 3};
    const y = googObject.createImmutableView(x);
    assertThrows(() => {
      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      y.propA = 4;
    });
    assertThrows(() => {
      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      y.propB = 4;
    });
  },

  testEmptyObjectsAreEqual() {
    assertTrue(googObject.equals({}, {}));
  },

  testObjectsWithDifferentKeysAreUnequal() {
    assertFalse(googObject.equals({'a': 1}, {'b': 1}));
  },

  testObjectsWithDifferentValuesAreUnequal() {
    assertFalse(googObject.equals({'a': 1}, {'a': 2}));
  },

  testObjectsWithSameKeysAndValuesAreEqual() {
    assertTrue(googObject.equals({'a': 1}, {'a': 1}));
  },

  testObjectsWithSameKeysInDifferentOrderAreEqual() {
    assertTrue(googObject.equals({'a': 1, 'b': 2}, {'b': 2, 'a': 1}));
  },

  testGetAllPropertyNames_enumerableProperties() {
    const obj = {a: function() {}, b: 'b', c: function(x) {}};
    assertSameElements(['a', 'b', 'c'], googObject.getAllPropertyNames(obj));
  },

  testGetAllPropertyNames_nonEnumerableProperties() {
    const obj = {};
    try {
      Object.defineProperty(obj, 'foo', {value: 'bar', enumerable: false});
    } catch (ex) {
      // IE8 doesn't allow Object.defineProperty on non-DOM elements.
      if (ex.message == 'Object doesn\'t support this action') {
        return;
      }
    }

    const expected = (Object.getOwnPropertyNames !== undefined) ? ['foo'] : [];
    assertSameElements(expected, googObject.getAllPropertyNames(obj));
  },

  testGetAllPropertyNames_inheritedProperties() {
    const parent = function() {};
    parent.prototype.a = null;

    const child = function() {};
    goog.inherits(child, parent);
    child.prototype.b = null;

    const expected = ['a', 'b'];
    if (Object.getOwnPropertyNames !== undefined) {
      expected.push('constructor');
    }

    assertSameElements(
        expected, googObject.getAllPropertyNames(child.prototype));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetAllPropertyNames_es6ClassProperties() {
    // Create an ES6 class via eval so we can bail out if it's a syntax error in
    // browsers that don't support ES6 classes.
    let Foo, Bar;
    try {
      eval(
          'Foo = class {' +
          '  a() {}' +
          '};' +
          'Foo.prototype.b = null;' +
          'Bar = class extends Foo {' +
          '  c() {}' +
          '  static d() {}' +
          '};');
    } catch (e) {
      if (e instanceof SyntaxError) {
        return;
      }
    }

    assertSameElements(
        ['a', 'b', 'constructor'],
        googObject.getAllPropertyNames(Foo.prototype));
    assertSameElements(
        ['a', 'b', 'c', 'constructor'],
        googObject.getAllPropertyNames(Bar.prototype));

    const expectedBarProperties = ['d', 'prototype', 'length', 'name'];

    // Some versions of Firefox don't find the name property via
    // getOwnPropertyNames.
    if (!googArray.contains(Object.getOwnPropertyNames(Bar), 'name')) {
      googArray.remove(expectedBarProperties, 'name');
    }

    assertSameElements(
        expectedBarProperties, googObject.getAllPropertyNames(Bar));
  },

  testGetAllPropertyNames_includeObjectPrototype() {
    const obj = {a: function() {}, b: 'b', c: function(x) {}};

    // There's slightly different behavior depending on what APIs the browser
    // under test supports.
    /** @suppress {checkTypes} suppression added to enable type checking */
    const additionalProps = !!Object.getOwnPropertyNames ?
        Object.getOwnPropertyNames(Object.prototype) :
        [];
    // __proto__ is a bit special and should be excluded from the result set.
    googArray.remove(additionalProps, '__proto__');

    assertSameElements(
        ['a', 'b', 'c'].concat(additionalProps),
        googObject.getAllPropertyNames(obj, true));
  },

  testGetAllPropertyNames_includeFunctionPrototype() {
    const obj = function() {};
    obj.a = () => {};

    // There's slightly different behavior depending on what APIs the browser
    // under test supports.
    const additionalProps = !!Object.getOwnPropertyNames ?
        Object.getOwnPropertyNames(Function.prototype) :
        [];

    const expectedElements = ['a'].concat(additionalProps);
    googArray.removeDuplicates(expectedElements);

    // There's slightly different behavior depending on what APIs the browser
    // under test supports.
    let results = googObject.getAllPropertyNames(obj, false, true);
    results = results.filter(word => word != 'prototype');

    assertSameElements(expectedElements.sort(), results.sort());
  },

  testGetSuperClass_es5() {
    function A() {}
    function B() {}
    goog.inherits(B, A);

    assertEquals(A, googObject.getSuperClass(B));
    assertEquals(Object, googObject.getSuperClass(A));
    assertEquals(null, googObject.getSuperClass(Object));
  },

  testGetSuperClass_es6() {
    class A {}
    class B extends A {}

    assertEquals(A, googObject.getSuperClass(B));
    assertEquals(Object, googObject.getSuperClass(A));
    assertEquals(null, googObject.getSuperClass(Object));
  },
});
