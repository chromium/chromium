/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.MapTest');
goog.setTestOnly();

const StructsMap = goog.require('goog.ui.Map');
// const googIter = goog.require('goog.iter');
const structs = goog.require('goog.structs');
const testSuite = goog.require('goog.testing.testSuite');

function stringifyMap(m) {
  const keys = structs.getKeys(m);
  let s = '';
  for (let i = 0; i < keys.length; i++) {
    s += keys[i] + m[keys[i]];
  }
  return s;
}

function getMap() {
  const m = new StructsMap;
  m.set('a', 0);
  m.set('b', 1);
  m.set('c', 2);
  m.set('d', 3);
  return m;
}

testSuite({
  testGetCount() {
    const m = getMap();
    assertEquals('count, should be 4', m.getCount(), 4);
    m.remove('d');
    assertEquals('count, should be 3', m.getCount(), 3);
  },

  testKeys() {
    const m = getMap();
    assertEquals(
        'getKeys, The keys should be a,b,c', m.getKeys().join(','), 'a,b,c,d');
  },

  testValues() {
    const m = getMap();
    assertEquals(
        'getValues, The values should be 0,1,2', m.getValues().join(','),
        '0,1,2,3');
  },

  testContainsKey() {
    const m = getMap();
    assertTrue('containsKey, Should contain the \'a\' key', m.containsKey('a'));
    assertFalse(
        'containsKey, Should not contain the \'e\' key', m.containsKey('e'));
  },

  testClear() {
    const m = getMap();
    m.clear();
    assertTrue('cleared so it should be empty', m.isEmpty());
    assertTrue(
        'cleared so it should not contain \'a\' key', !m.containsKey('a'));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAddAll() {
    const m = new StructsMap;
    m.addAll({a: 0, b: 1, c: 2, d: 3});
    assertTrue('addAll so it should not be empty', !m.isEmpty());
    assertTrue('addAll so it should contain \'c\' key', m.containsKey('c'));

    const m2 = new StructsMap;
    m2.addAll(m);
    assertTrue('addAll so it should not be empty', !m2.isEmpty());
    assertTrue('addAll so it should contain \'c\' key', m2.containsKey('c'));

    m2.addAll(null);  // Ensure that passing a null object does not err.
  },

  testConstructor() {
    const m = getMap();
    const m2 = new StructsMap(m);
    assertTrue('constr with Map so it should not be empty', !m2.isEmpty());
    assertTrue(
        'constr with Map so it should contain \'c\' key', m2.containsKey('c'));
  },

  testConstructorWithVarArgs() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    let m = new StructsMap('a', 1);
    assertTrue('constr with var_args so it should not be empty', !m.isEmpty());
    assertEquals('constr with var_args', 1, m.get('a'));

    /** @suppress {checkTypes} suppression added to enable type checking */
    m = new StructsMap('a', 1, 'b', 2);
    assertTrue('constr with var_args so it should not be empty', !m.isEmpty());
    assertEquals('constr with var_args', 1, m.get('a'));
    assertEquals('constr with var_args', 2, m.get('b'));

    assertThrows('Odd number of arguments is not allowed', () => {
      /** @suppress {checkTypes} suppression added to enable type checking */
      const m = new StructsMap('a', 1, 'b');
    });
  },

  testClone() {
    const m = getMap();
    const m2 = m.clone();
    assertTrue('clone so it should not be empty', !m2.isEmpty());
    assertTrue('clone so it should contain \'c\' key', m2.containsKey('c'));
  },

  testRemove() {
    const m = new StructsMap();
    for (let i = 0; i < 1000; i++) {
      m.set(i, 'foo');
    }

    for (let i = 0; i < 1000; i++) {
      assertTrue(m.getKeys().length <= 2 * m.getCount());
      m.remove(i);
    }
    assertTrue(m.isEmpty());
    assertEquals('', m.getKeys().join(''));
  },

  testForEach() {
    const m = getMap();
    let s = '';
    structs.forEach(m, (val, key, m2) => {
      assertNotUndefined(key);
      assertEquals(m, m2);
      s += key + val;
    });
    assertEquals(s, 'a0b1c2d3');
  },

  testFilter() {
    const m = getMap();

    const m2 = structs.filter(m, (val, key, m3) => {
      assertNotUndefined(key);
      assertEquals(m, m3);
      return val > 1;
    });
    assertEquals(stringifyMap(m2), 'c2d3');
  },

  testMap() {
    const m = getMap();
    const m2 = structs.map(m, (val, key, m3) => {
      assertNotUndefined(key);
      assertEquals(m, m3);
      return val * val;
    });
    assertEquals(stringifyMap(m2), 'a0b1c4d9');
  },

  testSome() {
    const m = getMap();
    const b = structs.some(m, (val, key, m2) => {
      assertNotUndefined(key);
      assertEquals(m, m2);
      return val > 1;
    });
    assertTrue(b);

    const b2 = structs.some(m, (val, key, m2) => {
      assertNotUndefined(key);
      assertEquals(m, m2);
      return val > 100;
    });
    assertFalse(b2);
  },

  testEvery() {
    const m = getMap();
    let b = structs.every(m, (val, key, m2) => {
      assertNotUndefined(key);
      assertEquals(m, m2);
      return val >= 0;
    });
    assertTrue(b);
    b = structs.every(m, (val, key, m2) => {
      assertNotUndefined(key);
      assertEquals(m, m2);
      return val > 1;
    });
    assertFalse(b);
  },

  testContainsValue() {
    const m = getMap();
    assertTrue(m.containsValue(3));
    assertFalse(m.containsValue(4));
  },

  testObjectProperties() {
    const m = new StructsMap;

    assertEquals(m.get('toString'), undefined);
    assertEquals(m.get('valueOf'), undefined);
    assertEquals(m.get('eval'), undefined);
    assertEquals(m.get('toSource'), undefined);
    assertEquals(m.get('prototype'), undefined);
    assertEquals(m.get(':foo'), undefined);

    m.set('toString', 'once');
    m.set('valueOf', 'upon');
    m.set('eval', 'a');
    m.set('toSource', 'midnight');
    m.set('prototype', 'dreary');
    m.set('hasOwnProperty', 'dark');
    m.set(':foo', 'happy');

    assertEquals(m.get('toString'), 'once');
    assertEquals(m.get('valueOf'), 'upon');
    assertEquals(m.get('eval'), 'a');
    assertEquals(m.get('toSource'), 'midnight');
    assertEquals(m.get('prototype'), 'dreary');
    assertEquals(m.get('hasOwnProperty'), 'dark');
    assertEquals(m.get(':foo'), 'happy');

    const keys = m.getKeys().join(',');
    assertEquals(
        keys, 'toString,valueOf,eval,toSource,prototype,hasOwnProperty,:foo');

    const values = m.getValues().join(',');
    assertEquals(values, 'once,upon,a,midnight,dreary,dark,happy');
  },

  testDuplicateKeys() {
    const m = new StructsMap;

    m.set('a', 1);
    m.set('b', 2);
    m.set('c', 3);
    m.set('d', 4);
    m.set('e', 5);
    m.set('f', 6);
    assertEquals(6, m.getKeys().length);
    m.set('foo', 1);
    assertEquals(7, m.getKeys().length);
    m.remove('foo');
    assertEquals(6, m.getKeys().length);
    m.set('foo', 2);
    assertEquals(7, m.getKeys().length);
    m.remove('foo');
    m.set('foo', 3);
    m.remove('foo');
    m.set('foo', 4);
    assertEquals(7, m.getKeys().length);
  },

  testToObject() {
    Object.prototype.b = 0;
    try {
      const map = new StructsMap();
      map.set('a', 0);
      const obj = map.toObject();
      assertTrue('object representation has key "a"', obj.hasOwnProperty('a'));
      assertFalse(
          'object representation does not have key "b"',
          obj.hasOwnProperty('b'));
      assertEquals('value for key "a"', 0, obj['a']);
    } finally {
      delete Object.prototype.b;
    }
  },

  testEqualsWithSameObject() {
    const map1 = getMap();
    assertTrue('maps are the same object', map1.equals(map1));
  },

  testEqualsWithDifferentSizeMaps() {
    const map1 = getMap();
    const map2 = new StructsMap();

    assertFalse('maps are different sizes', map1.equals(map2));
  },

  testEqualsWithDefaultEqualityFn() {
    let map1 = new StructsMap();
    let map2 = new StructsMap();

    assertTrue('maps are both empty', map1.equals(map2));

    map1 = getMap();
    map2 = getMap();
    assertTrue('maps are the same', map1.equals(map2));

    map2.set('d', '3');
    assertFalse('maps have 3 and \'3\'', map1.equals(map2));
  },

  testEqualsWithCustomEqualityFn() {
    const map1 = new StructsMap();
    const map2 = new StructsMap();

    map1.set('a', 0);
    map1.set('b', 1);

    map2.set('a', '0');
    map2.set('b', '1');

    const equalsFn = (a, b) => a == b;

    assertTrue('maps are equal with ==', map1.equals(map2, equalsFn));
  },
});
