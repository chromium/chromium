/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.structs.MultimapTest');
goog.setTestOnly();

const Multimap = goog.require('goog.labs.structs.Multimap');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let map;

function shouldRunTests() {
  if (userAgent.IE) {
    return userAgent.isVersionOrHigher(9);
  }
  return true;
}

/**
 * @param {T} entry
 * @param {!Array<T>} entryList
 * @template T
 */
function assertContainsEntry(entry, entryList) {
  for (let i = 0; i < entryList.length; ++i) {
    if (entry[0] == entryList[i][0] && entry[1] === entryList[i][1]) {
      return;
    }
  }
  fail(`Did not find entry: ${entry} in: ${entryList}`);
}
testSuite({
  setUp() {
    map = new Multimap();
  },

  testGetCountWithEmptyMultimap() {
    assertEquals(0, map.getCount());
    assertTrue(map.isEmpty());
  },

  testClone() {
    map.add('k', 'v');
    map.addAllValues('k2', ['v', 'v1', 'v2']);

    const map2 = map.clone();

    assertSameElements(['v'], map.get('k'));
    assertSameElements(['v', 'v1', 'v2'], map.get('k2'));

    assertSameElements(['v'], map2.get('k'));
    assertSameElements(['v', 'v1', 'v2'], map2.get('k2'));
  },

  testAdd() {
    map.add('key', 'v');
    assertEquals(1, map.getCount());
    map.add('key', 'v2');
    assertEquals(2, map.getCount());
    map.add('key', 'v3');
    assertEquals(3, map.getCount());

    const values = map.get('key');
    assertEquals(3, values.length);
    assertContains('v', values);
    assertContains('v2', values);
    assertContains('v3', values);
  },

  testAddValues() {
    map.addAllValues('key', ['v', 'v2', 'v3']);
    assertSameElements(['v', 'v2', 'v3'], map.get('key'));

    map.add('key2', 'a');
    map.addAllValues('key2', ['v', 'v2', 'v3']);
    assertSameElements(['a', 'v', 'v2', 'v3'], map.get('key2'));
  },

  testAddAllWithMultimap() {
    map.add('k', 'v');
    map.addAllValues('k2', ['v', 'v1', 'v2']);

    const map2 = new Multimap();
    map2.add('k2', 'v');
    map2.addAllValues('k3', ['a', 'a1', 'a2']);

    map.addAllFromMultimap(map2);
    assertSameElements(['v'], map.get('k'));
    assertSameElements(['v', 'v1', 'v2', 'v'], map.get('k2'));
    assertSameElements(['a', 'a1', 'a2'], map.get('k3'));
  },

  testReplaceValues() {
    map.add('key', 'v');
    map.add('key', 'v2');

    map.replaceValues('key', [0, 1, 2]);
    assertSameElements([0, 1, 2], map.get('key'));
    assertEquals(3, map.getCount());

    map.replaceValues('key', ['v']);
    assertSameElements(['v'], map.get('key'));
    assertEquals(1, map.getCount());

    map.replaceValues('key', []);
    assertSameElements([], map.get('key'));
    assertEquals(0, map.getCount());
  },

  testRemove() {
    map.add('key', 'v');
    map.add('key', 'v2');
    map.add('key', 'v3');

    assertTrue(map.remove('key', 'v'));
    let values = map.get('key');
    assertEquals(2, map.getCount());
    assertEquals(2, values.length);
    assertContains('v2', values);
    assertContains('v3', values);
    assertFalse(map.remove('key', 'v'));

    assertTrue(map.remove('key', 'v2'));
    values = map.get('key');
    assertEquals(1, map.getCount());
    assertEquals(1, values.length);
    assertContains('v3', values);
    assertFalse(map.remove('key', 'v2'));

    assertTrue(map.remove('key', 'v3'));
    map.remove('key', 'v3');
    assertTrue(map.isEmpty());
    assertEquals(0, map.get('key').length);
    assertFalse(map.remove('key', 'v2'));
  },

  testRemoveWithNaN() {
    map.add('key', NaN);
    map.add('key', NaN);

    assertTrue(map.remove('key', NaN));
    const values = map.get('key');
    assertEquals(1, values.length);
    assertTrue(isNaN(values[0]));

    assertTrue(map.remove('key', NaN));
    assertEquals(0, map.get('key').length);
    assertFalse(map.remove('key', NaN));
  },

  testRemoveWithNegativeZero() {
    map.add('key', 0);
    map.add('key', -0);

    assertTrue(map.remove('key', -0));
    let values = map.get('key');
    assertEquals(1, values.length);
    assertTrue(1 / values[0] === 1 / 0);
    assertFalse(map.remove('key', -0));

    map.add('key', -0);

    assertTrue(map.remove('key', 0));
    values = map.get('key');
    assertEquals(1, values.length);
    assertTrue(1 / values[0] === 1 / -0);
    assertFalse(map.remove('key', 0));

    assertTrue(map.remove('key', -0));
    assertEquals(0, map.get('key').length);
  },

  testRemoveAll() {
    map.add('key', 'v');
    map.add('key', 'v2');
    map.add('key', 'v3');
    map.add('key', 'v4');
    map.add('key2', 'v');

    assertTrue(map.removeAll('key'));
    assertSameElements([], map.get('key'));
    assertSameElements(['v'], map.get('key2'));
    assertFalse(map.removeAll('key'));
    assertEquals(1, map.getCount());

    assertTrue(map.removeAll('key2'));
    assertSameElements([], map.get('key2'));
    assertFalse(map.removeAll('key2'));
    assertTrue(map.isEmpty());
  },

  testAddWithDuplicateValue() {
    map.add('key', 'v');
    map.add('key', 'v');
    map.add('key', 'v');
    assertArrayEquals(['v', 'v', 'v'], map.get('key'));
  },

  testContainsEntry() {
    assertFalse(map.containsEntry('k', 'v'));
    assertFalse(map.containsEntry('k', 'v2'));
    assertFalse(map.containsEntry('k2', 'v'));

    map.add('k', 'v');
    assertTrue(map.containsEntry('k', 'v'));
    assertFalse(map.containsEntry('k', 'v2'));
    assertFalse(map.containsEntry('k2', 'v'));

    map.add('k', 'v2');
    assertTrue(map.containsEntry('k', 'v'));
    assertTrue(map.containsEntry('k', 'v2'));
    assertFalse(map.containsEntry('k2', 'v'));

    map.add('k2', 'v');
    assertTrue(map.containsEntry('k', 'v'));
    assertTrue(map.containsEntry('k', 'v2'));
    assertTrue(map.containsEntry('k2', 'v'));
  },

  testContainsKey() {
    assertFalse(map.containsKey('k'));
    assertFalse(map.containsKey('k2'));

    map.add('k', 'v');
    assertTrue(map.containsKey('k'));
    map.add('k2', 'v');
    assertTrue(map.containsKey('k2'));

    map.remove('k', 'v');
    assertFalse(map.containsKey('k'));
    map.remove('k2', 'v');
    assertFalse(map.containsKey('k2'));
  },

  testContainsValue() {
    assertFalse(map.containsValue('v'));
    assertFalse(map.containsValue('v2'));

    map.add('key', 'v');
    assertTrue(map.containsValue('v'));
    map.add('key', 'v2');
    assertTrue(map.containsValue('v2'));
  },

  testGetEntries() {
    map.add('key', 'v');
    map.add('key', 'v2');
    map.add('key2', 'v3');

    const entries = map.getEntries();
    assertEquals(3, entries.length);
    assertContainsEntry(['key', 'v'], entries);
    assertContainsEntry(['key', 'v2'], entries);
    assertContainsEntry(['key2', 'v3'], entries);
  },

  testGetKeys() {
    map.add('key', 'v');
    map.add('key', 'v2');
    map.add('key2', 'v3');
    map.add('key3', 'v4');
    map.removeAll('key3');

    assertSameElements(['key', 'key2'], map.getKeys());
  },

  testGetValues() {
    map.add('key', 'v');
    map.add('key', 'v2');
    map.add('key2', 'v2');
    map.add('key3', 'v4');
    map.removeAll('key3');

    assertSameElements(['v', 'v2', 'v2'], map.getValues());
  },

  testGetReturnsDefensiveCopyOfUnderlyingData() {
    map.add('key', 'v');
    map.add('key', 'v2');
    map.add('key', 'v3');

    const values = map.get('key');
    values.push('v4');
    assertFalse(map.containsEntry('key', 'v4'));
  },

  testClear() {
    map.add('key', 'v');
    map.add('key', 'v2');
    map.add('key2', 'v3');

    map.clear();
    assertTrue(map.isEmpty());
    assertSameElements([], map.getEntries());
  },
});
