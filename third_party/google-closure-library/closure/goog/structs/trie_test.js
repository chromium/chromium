/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.structs.TrieTest');
goog.setTestOnly();

const Trie = goog.require('goog.structs.Trie');
const googObject = goog.require('goog.object');
const structs = goog.require('goog.structs');
const testSuite = goog.require('goog.testing.testSuite');

function makeTrie() {
  const trie = new Trie();
  trie.add('hello', 1);
  trie.add('hi', 'howdy');
  trie.add('', 'an empty string key');
  trie.add('empty value', '');
  trie.add('zero', 0);
  trie.add('object', {});
  trie.add('null', null);
  trie.add('hello, world', 2);
  trie.add('world', {});
  return trie;
}

function checkTrie(trie) {
  assertEquals('get, should be 1', trie.get('hello'), 1);
  assertEquals('get, should be "howdy"', trie.get('hi'), 'howdy');
  assertEquals(
      'get, should be "an empty string key"', trie.get(''),
      'an empty string key');
  assertEquals('get, should be ""', trie.get('empty value'), '');
  assertEquals('get, should be ""', typeof trie.get('empty value'), 'string');
  assertEquals('get, should be an object', typeof trie.get('object'), 'object');
  assertEquals('get, should be 0', trie.get('zero'), 0);
  assertEquals('get "null", should be null', trie.get('null'), null);
  assertEquals('get, should be 2', trie.get('hello, world'), 2);
  assertEquals('get, should be an object', typeof trie.get('world'), 'object');
}

testSuite({
  testTrieFormation() {
    const t = makeTrie();
    checkTrie(t);
  },

  testFailureOfMultipleAdds() {
    let t = new Trie();
    t.add('hello', 'testing');
    assertThrows('Error should be thrown when same key added twice.', () => {
      t.add('hello', 'test');
    });

    t = new Trie();
    t.add('null', null);
    assertThrows('Error should be thrown when same key added twice.', () => {
      t.add('null', 'hi!');
    });

    t = new Trie();
    t.add('null', 'blah');
    assertThrows('Error should be thrown when same key added twice.', () => {
      t.add('null', null);
    });
  },

  testTrieClone() {
    const trieOne = makeTrie();
    const trieTwo = new Trie(trieOne);
    checkTrie(trieTwo);
  },

  testTrieFromObject() {
    const someObject = {
      'hello': 1,
      'hi': 'howdy',
      '': 'an empty string key',
      'empty value': '',
      'object': {},
      'zero': 0,
      'null': null,
      'hello, world': 2,
      'world': {},
    };
    const trie = new Trie(someObject);
    checkTrie(trie);
  },

  testTrieGetValues() {
    const trie = makeTrie();
    const values = trie.getValues();
    assertTrue(
        'getValues, should contain "howdy"',
        googObject.contains(values, 'howdy'));
    assertTrue('getValues, should contain 1', googObject.contains(values, 1));
    assertTrue('getValues, should contain 0', googObject.contains(values, 0));
    assertTrue('getValues, should contain ""', googObject.contains(values, ''));
    assertTrue(
        'getValues, should contain null', googObject.contains(values, null));
    assertEquals(
        'goog.structs.getCount(getValues()) should be 9',
        structs.getCount(values), 9);
  },

  testTrieGetKeys() {
    const trie = makeTrie();
    const keys = trie.getKeys();
    assertTrue(
        'getKeys, should contain "hello"', googObject.contains(keys, 'hello'));
    assertTrue(
        'getKeys, should contain "empty value"',
        googObject.contains(keys, 'empty value'));
    assertTrue('getKeys, should contain ""', googObject.contains(keys, ''));
    assertTrue(
        'getKeys, should contain "zero"', googObject.contains(keys, 'zero'));
    assertEquals(
        'goog.structs.getCount(getKeys()) should be 9', structs.getCount(keys),
        9);
  },

  testTrieCount() {
    const trieOne = makeTrie();
    const trieTwo = new Trie();
    assertEquals('count, should be 9', trieOne.getCount(), 9);
    assertEquals('count, should be 0', trieTwo.getCount(), 0);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemoveKeyFromTrie() {
    const trie = new Trie();
    trie.add('key1', 'value1');
    trie.add('key2', 'value2');
    trie.add('ke', 'value3');
    trie.add('zero', 0);
    trie.remove('key2');
    assertEquals('get "key1", should be "value1"', trie.get('key1'), 'value1');
    assertUndefined('get "key2", should be undefined', trie.get('key2'));
    trie.remove('zero');
    assertUndefined('get "zero", should be undefined', trie.get('zero'));
    trie.remove('ke');
    assertUndefined('get "ke", should be undefined', trie.get('ke'));
    assertEquals('get "key1", should be "value1"', trie.get('key1'), 'value1');
    trie.add('a', 'value4');
    assertTrue(
        'testing internal structure, a should be a child',
        'a' in trie.childNodes_);
    trie.remove('a');
    assertFalse(
        'testing internal structure, a should no longer be a child',
        'a' in trie.childNodes_);

    trie.add('xyza', 'value');
    trie.remove('xyza');
    assertFalse('Should not have "x"', 'x' in trie.childNodes_);

    trie.add('xyza', null);
    assertTrue('Should have "x"', 'x' in trie.childNodes_);
    trie.remove('xyza');
    assertFalse('Should not have "x"', 'x' in trie.childNodes_);

    trie.add('xyza', 'value');
    trie.add('xb', 'value');
    trie.remove('xyza');
    assertTrue('get "x" should be defined', 'x' in trie.childNodes_);
    assertFalse(
        'get "y" should be undefined',
        'y' in trie.childNodes_['x'].childNodes_);

    trie.add('akey', 'value1');
    trie.add('akey1', 'value2');
    trie.remove('akey1');
    assertEquals('get "akey", should be "value1"', 'value1', trie.get('akey'));
    assertUndefined('get "akey1", should be undefined', trie.get('akey1'));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemoveKeyFromTrieWithNulls() {
    const trie = new Trie();
    trie.add('key1', null);
    trie.add('key2', 'value2');
    trie.add('ke', 'value3');
    trie.add('zero', 0);
    trie.remove('key2');
    assertEquals('get "key1", should be null', trie.get('key1'), null);
    assertUndefined('get "key2", should be undefined', trie.get('key2'));
    trie.remove('zero');
    assertUndefined('get "zero", should be undefined', trie.get('zero'));
    trie.remove('ke');
    assertUndefined('get "ke", should be undefined', trie.get('ke'));
    assertEquals('get "key1", should be null', trie.get('key1'), null);
    trie.add('a', 'value4');
    assertTrue(
        'testing internal structure, a should be a child',
        'a' in trie.childNodes_);
    trie.remove('a');
    assertFalse(
        'testing internal structure, a should no longer be a child',
        'a' in trie.childNodes_);

    trie.add('xyza', null);
    trie.add('xb', 'value');
    trie.remove('xyza');
    assertTrue('Should have "x"', 'x' in trie.childNodes_);
    assertFalse(
        'Should not have "y"', 'y' in trie.childNodes_['x'].childNodes_);
  },

  testRemoveKeyException() {
    const trie = new Trie();
    trie.add('abcdefg', 'value');
    trie.add('abcz', 'value');
    trie.add('abc', 'value');

    assertThrows(
        'Remove should throw an error on removal of non-existent key', () => {
          trie.remove('abcdefge');
        });
  },

  testTrieIsEmpty() {
    const trieOne = new Trie();
    const trieTwo = makeTrie();
    assertTrue('isEmpty, should be empty', trieOne.isEmpty());
    assertFalse('isEmpty, should not be empty', trieTwo.isEmpty());
    trieOne.add('', 1);
    assertFalse('isEmpty, should not be empty', trieTwo.isEmpty());
    trieOne.remove('');
    assertTrue('isEmpty, should be empty', trieOne.isEmpty());
    trieOne.add('', 1);
    trieOne.add('a', 1);
    trieOne.remove('a');
    assertFalse('isEmpty, should not be empty', trieOne.isEmpty());
    trieOne.remove('');
    assertTrue('isEmpty, should be empty', trieOne.isEmpty());
    trieOne.add('', 1);
    trieOne.add('a', 1);
    trieOne.remove('');
    assertFalse('isEmpty, should not be empty', trieOne.isEmpty());
    trieOne.remove('a');
    assertTrue('isEmpty, should be empty', trieOne.isEmpty());
  },

  testTrieClear() {
    const trie = new Trie();
    trie.add('key1', 'value1');
    trie.add('key2', 'value2');
    trie.add('key3', null);
    trie.clear();
    assertUndefined('get key1, should be undefined', trie.get('key1'));
    assertUndefined('get key2, should be undefined', trie.get('key2'));
    assertUndefined('get key3, should be undefined', trie.get('key3'));
  },

  testTrieContainsKey() {
    const trie = makeTrie();
    assertTrue(
        'containsKey, should contain "hello"', trie.containsKey('hello'));
    assertTrue('containsKey, should contain "hi"', trie.containsKey('hi'));
    assertTrue('containsKey, should contain ""', trie.containsKey(''));
    assertTrue(
        'containsKey, should contain "empty value"',
        trie.containsKey('empty value'));
    assertTrue(
        'containsKey, should contain "object"', trie.containsKey('object'));
    assertTrue('containsKey, should contain "zero"', trie.containsKey('zero'));
    assertTrue('containsKey, should contain "null"', trie.containsKey('null'));
    assertFalse(
        'containsKey, should not contain "blah"', trie.containsKey('blah'));
    trie.remove('');
    trie.remove('hi');
    trie.remove('zero');
    trie.remove('null');
    assertFalse(
        'containsKey, should not contain "zero"', trie.containsKey('zero'));
    assertFalse('containsKey, should not contain ""', trie.containsKey(''));
    assertFalse('containsKey, should not contain "hi"', trie.containsKey('hi'));
    assertFalse(
        'containsKey, should not contain "null"', trie.containsKey('null'));
  },

  testTrieContainsPrefix() {
    // Empty trie.
    let trie = new Trie();
    assertFalse(
        'containsPrefix, should not contain ""', trie.containsPrefix(''));
    assertFalse(
        'containsPrefix, should not contain "any"', trie.containsPrefix('any'));
    trie.add('key', 'value');
    assertTrue('containsPrefix, should contain ""', trie.containsPrefix(''));

    // Non-empty trie.
    trie = makeTrie();
    assertTrue('containsPrefix, should contain ""', trie.containsPrefix(''));
    assertFalse(
        'containsPrefix, should not contain "blah"',
        trie.containsPrefix('blah'));
    assertTrue('containsPrefix, should contain "h"', trie.containsPrefix('h'));
    assertTrue(
        'containsPrefix, should contain "hello"', trie.containsPrefix('hello'));
    assertTrue(
        'containsPrefix, should contain "hello, world"',
        trie.containsPrefix('hello, world'));
    assertFalse(
        'containsPrefix, should not contain "hello, world!"',
        trie.containsPrefix('hello, world!'));
    assertTrue(
        'containsPrefix, should contain "nu"', trie.containsPrefix('nu'));
    assertTrue(
        'containsPrefix, should contain "null"', trie.containsPrefix('null'));
    assertTrue(
        'containsPrefix, should contain "empty value"',
        trie.containsPrefix('empty value'));

    // Remove nodes.
    trie.remove('');
    assertTrue('containsPrefix, should contain ""', trie.containsPrefix(''));
    trie.remove('hi');
    assertTrue('containsPrefix, should contain "h"', trie.containsPrefix('h'));
    assertFalse(
        'containsPrefix, should not contain "hi"', trie.containsPrefix('hi'));
    trie.remove('hello');
    trie.remove('hello, world');
    assertFalse(
        'containsPrefix, should not contain "h"', trie.containsPrefix('h'));
    assertFalse(
        'containsPrefix, should not contain "hello"',
        trie.containsPrefix('hello'));

    // Remove all nodes.
    trie.remove('empty value');
    trie.remove('zero');
    trie.remove('object');
    trie.remove('null');
    trie.remove('world');
    assertFalse(
        'containsPrefix, should not contain ""', trie.containsPrefix(''));
    assertFalse(
        'containsPrefix, should not contain "h"', trie.containsPrefix('h'));
    assertFalse(
        'containsPrefix, should not contain "hi"', trie.containsPrefix('hi'));

    // Add some new nodes.
    trie.add('hi', 'value');
    trie.add('null', 'value');
    assertTrue('containsPrefix, should contain ""', trie.containsPrefix(''));
    assertTrue('containsPrefix, should contain "h"', trie.containsPrefix('h'));
    assertTrue(
        'containsPrefix, should contain "hi"', trie.containsPrefix('hi'));
    assertFalse(
        'containsPrefix, should not contain "hello"',
        trie.containsPrefix('hello'));
    assertFalse(
        'containsPrefix, should not contain "zero"',
        trie.containsPrefix('zero'));

    // Clear the trie.
    trie.clear();
    assertFalse(
        'containsPrefix, should not contain ""', trie.containsPrefix(''));
    assertFalse(
        'containsPrefix, should not contain "h"', trie.containsPrefix('h'));
    assertFalse(
        'containsPrefix, should not contain "hi"', trie.containsPrefix('hi'));
  },

  testTrieContainsValue() {
    const trie = makeTrie();
    assertTrue(
        'containsValue, should be true, should contain 1',
        trie.containsValue(1));
    assertTrue(
        'containsValue, should be true, should contain "howdy"',
        trie.containsValue('howdy'));
    assertTrue(
        'containsValue, should be true, should contain ""',
        trie.containsValue(''));
    assertTrue(
        'containsValue, should be true, should contain 0',
        trie.containsValue(0));
    assertTrue(
        'containsValue, should be true, should contain null',
        trie.containsValue(null));
    assertTrue(
        'containsValue, should be true, should ' +
            'contain "an empty string key"',
        trie.containsValue('an empty string key'));
    assertFalse(
        'containsValue, should be false, should not contain "blah"',
        trie.containsValue('blah'));
    trie.remove('empty value');
    trie.remove('zero');
    assertFalse(
        'containsValue, should be false, should not contain 0',
        trie.containsValue(0));
    assertFalse(
        'containsValue, should be false, should not contain ""',
        trie.containsValue(''));
  },

  testTrieHandlingOfEmptyStrings() {
    const trie = new Trie();
    assertEquals('get, should be undefined', trie.get(''), undefined);
    assertFalse('containsValue, should be false', trie.containsValue(''));
    assertFalse('containsKey, should be false', trie.containsKey(''));
    trie.add('', 'test');
    trie.add('test2', '');
    assertTrue('containsValue, should be true', trie.containsValue(''));
    assertTrue('containsKey, should be true', trie.containsKey(''));
    assertEquals('get, should be "test"', trie.get(''), 'test');
    assertEquals('get, should be ""', trie.get('test2'), '');
    trie.remove('');
    trie.remove('test2');
    assertEquals('get, should be undefined', trie.get(''), undefined);
    assertFalse('containsValue, should be false', trie.containsValue(''));
    assertFalse('containsKey, should be false', trie.containsKey(''));
  },

  testPrefixOptionOnGetKeys() {
    const trie = new Trie();
    trie.add('abcdefg', 'one');
    trie.add('abcdefghijk', 'two');
    trie.add('abcde', 'three');
    trie.add('abcq', null);
    trie.add('abc', 'four');
    trie.add('xyz', 'five');
    assertEquals('getKeys, should be 1', trie.getKeys('xy').length, 1);
    assertEquals('getKeys, should be 1', trie.getKeys('xyz').length, 1);
    assertEquals('getKeys, should be 1', trie.getKeys('x').length, 1);
    assertEquals('getKeys, should be 4', trie.getKeys('abc').length, 5);
    assertEquals('getKeys, should be 2', trie.getKeys('abcdef').length, 2);
    assertEquals('getKeys, should be 0', trie.getKeys('abcdefgi').length, 0);
  },

  testGetKeyAndPrefixes() {
    const trie = makeTrie();
    // Note: trie has one of its keys as ''
    assertObjectEquals(
        {0: 'an empty string key', 4: {}},  //
        trie.getKeyAndPrefixes('world'));
    assertObjectEquals(
        {0: 'an empty string key', 4: 1},  //
        trie.getKeyAndPrefixes('hello'));
    assertObjectEquals(
        {0: 'an empty string key', 4: 1},  //
        trie.getKeyAndPrefixes('hello,'));
    assertObjectEquals(
        {0: 'an empty string key', 4: 1, 11: 2},  //
        trie.getKeyAndPrefixes('hello, world'));
    assertObjectEquals(
        {0: 'an empty string key'},  //
        trie.getKeyAndPrefixes('hell'));
  },

  testGetKeyAndPrefixesStartIndex() {
    const trie = new Trie();
    trie.add('abcdefg', 'one');
    trie.add('bcdefg', 'two');
    trie.add('abcdefghijk', 'three');
    trie.add('abcde', 'four');
    trie.add('abcq', null);
    trie.add('q', null);
    trie.add('abc', 'five');
    trie.add('xyz', 'six');
    assertEquals(
        'getKeyAndPrefixes, should be 3', 3,
        googObject.getCount(trie.getKeyAndPrefixes('abcdefg', 0)));
    assertEquals(
        'getKeyAndPrefixes, should be 1', 1,
        googObject.getCount(trie.getKeyAndPrefixes('abcdefg', 1)));
    assertEquals(
        'getKeyAndPrefixes, should be 1', 1,
        googObject.getCount(trie.getKeyAndPrefixes('abcq', 3)));
    assertEquals(
        'getKeyAndPrefixes, should be 0', 0,
        googObject.getCount(trie.getKeyAndPrefixes('abcd', 3)));
  },
});
