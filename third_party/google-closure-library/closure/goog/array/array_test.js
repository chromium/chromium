/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.arrayTest');
goog.setTestOnly();

const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const googArray = goog.require('goog.array');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

/**
 * @param {!IArrayLike<?>} expected
 * @param {!IArrayLike<?>} original
 */
function assertRemovedDuplicates(expected, original) {
  const tempArr = googArray.clone(original);
  googArray.removeDuplicates(tempArr);
  assertArrayEquals(expected, tempArr);
}

/**
 * @param {number} size
 * @return {!IArrayLike<!Object>}
 */
function buildSortedObjectArray(size) {
  const objectArray = [];
  for (let i = 0; i < size; i++) {
    objectArray.push({'name': `name_${i}`, 'id': 'id_' + (size - i)});
  }

  return objectArray;
}

/**
 * @param {!Array<number>} expect
 * @param {!Array<number>} array
 * @param {number} rotate
 */
function assertRotated(expect, array, rotate) {
  assertArrayEquals(expect, googArray.rotate(array, rotate));
}

testSuite({
  testArrayLast() {
    assertEquals(googArray.last([1, 2, 3]), 3);
    assertEquals(googArray.last([1]), 1);
    assertUndefined(googArray.last([]));
  },

  testArrayLastWhenDeleted() {
    const a = [1, 2, 3];
    delete a[2];
    assertUndefined(googArray.last(a));
  },

  testArrayIndexOf() {
    assertEquals(googArray.indexOf([0, 1, 2, 3], 1), 1);
    assertEquals(googArray.indexOf([0, 1, 1, 1], 1), 1);
    assertEquals(googArray.indexOf([0, 1, 2, 3], 4), -1);
    assertEquals(googArray.indexOf([0, 1, 2, 3], 1, 1), 1);
    assertEquals(googArray.indexOf([0, 1, 2, 3], 1, 2), -1);
    assertEquals(googArray.indexOf([0, 1, 2, 3], 1, -3), 1);
    assertEquals(googArray.indexOf([0, 1, 2, 3], 1, -2), -1);
  },

  testArrayIndexOfOmitsDeleted() {
    const a = [0, 1, 2, 3];
    delete a[1];
    delete a[3];
    assertEquals(googArray.indexOf(a, undefined), -1);
  },

  testArrayIndexOfString() {
    assertEquals(googArray.indexOf('abcd', 'd'), 3);
    assertEquals(googArray.indexOf('abbb', 'b', 2), 2);
    assertEquals(googArray.indexOf('abcd', 'e'), -1);
    assertEquals(googArray.indexOf('abcd', 'cd'), -1);
    assertEquals(googArray.indexOf('0123', 1), -1);
  },

  testArrayLastIndexOf() {
    assertEquals(googArray.lastIndexOf([0, 1, 2, 3], 1), 1);
    assertEquals(googArray.lastIndexOf([0, 1, 1, 1], 1), 3);
    assertEquals(googArray.lastIndexOf([0, 1, 1, 1], 1, 2), 2);
  },

  testArrayLastIndexOfOmitsDeleted() {
    const a = [0, 1, 2, 3];
    delete a[1];
    delete a[3];
    assertEquals(googArray.lastIndexOf(a, undefined), -1);
  },

  testArrayLastIndexOfString() {
    assertEquals(googArray.lastIndexOf('abcd', 'b'), 1);
    assertEquals(googArray.lastIndexOf('abbb', 'b'), 3);
    assertEquals(googArray.lastIndexOf('abbb', 'b', 2), 2);
    assertEquals(googArray.lastIndexOf('abcd', 'cd'), -1);
    assertEquals(googArray.lastIndexOf('0123', 1), -1);
  },

  testArrayForEachBasic() {
    let s = '';
    const a = ['a', 'b', 'c', 'd'];
    googArray.forEach(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('Index is not a number', 'number', typeof index);
      s += val + index;
    });
    assertEquals('a0b1c2d3', s);
  },

  testArrayForEachWithEmptyArray() {
    const a = new Array(100);
    googArray.forEach(a, (val, index, a2) => {
      fail('The function should not be called since no values were assigned.');
    });
  },

  testArrayForEachWithOnlySomeValuesAsigned() {
    let count = 0;
    const a = new Array(1000);
    a[100] = undefined;
    googArray.forEach(a, (val, index, a2) => {
      assertEquals(100, index);
      count++;
    });
    assertEquals(
        'Should only call function when a value of array was assigned.', 1,
        count);
  },

  testArrayForEachWithArrayLikeObject() {
    const counter = recordFunction();
    const a = {'length': 1, '0': 0, '100': 100, '101': 102};
    googArray.forEach(a, counter);
    assertEquals(
        'Number of calls should not exceed the value of its length', 1,
        counter.getCallCount());
  },

  testArrayForEachOmitsDeleted() {
    let s = '';
    const a = ['a', 'b', 'c', 'd'];
    delete a[1];
    delete a[3];
    googArray.forEach(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('number', typeof index);
      s += val + index;
    });
    assertEquals('a0c2', s);
  },

  testArrayForEachScope() {
    const scope = {};
    const a = ['a', 'b', 'c', 'd'];
    googArray.forEach(a, function(val, index, a2) {
      assertEquals(a, a2);
      assertEquals('number', typeof index);
      assertEquals(this, scope);
    }, scope);
  },

  testArrayForEachRight() {
    let s = '';
    const a = ['a', 'b', 'c', 'd'];
    googArray.forEachRight(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('number', typeof index);
      s += val + String(index);
    });
    assertEquals('d3c2b1a0', s);
  },

  testArrayForEachRightOmitsDeleted() {
    let s = '';
    const a = ['a', 'b', 'c', 'd'];
    delete a[1];
    delete a[3];
    googArray.forEachRight(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('number', typeof index);
      assertEquals('string', typeof val);
      s += val + String(index);
    });
    assertEquals('c2a0', s);
  },

  testArrayFilter() {
    let a = [0, 1, 2, 3];
    a = googArray.filter(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 1;
    });
    assertArrayEquals([2, 3], a);
  },

  testArrayFilterOmitsDeleted() {
    let a = [0, 1, 2, 3];
    delete a[1];
    delete a[3];
    a = googArray.filter(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('number', typeof val);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 1;
    });
    assertArrayEquals([2], a);
  },

  testArrayFilterPreservesValues() {
    let a = [0, 1, 2, 3];
    a = googArray.filter(a, (val, index, a2) => {
      assertEquals(a, a2);
      // sometimes functions might be evil and do something like this, but we
      // should still use the original values when returning the filtered array
      a2[index] = a2[index] - 1;
      return a2[index] >= 1;
    });
    assertArrayEquals([2, 3], a);
  },

  testArrayMap() {
    const a = [0, 1, 2, 3];
    const result = googArray.map(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val * val;
    });
    assertArrayEquals([0, 1, 4, 9], result);
  },

  testArrayMapOmitsDeleted() {
    const a = [0, 1, 2, 3];
    delete a[1];
    delete a[3];
    const result = googArray.map(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('number', typeof val);
      assertEquals('index is not a number', 'number', typeof index);
      return val * val;
    });
    const expected = [0, 1, 4, 9];
    delete expected[1];
    delete expected[3];

    assertArrayEquals(expected, result);
    assertFalse('1' in result);
    assertFalse('3' in result);
  },

  testArrayReduce() {
    const a = [0, 1, 2, 3];
    assertEquals(6, googArray.reduce(a, (rval, val, i, arr) => {
      assertEquals('number', typeof i);
      assertEquals(a, arr);
      return rval + val;
    }, 0));

    /** @const */
    const scope = {
      last: 0,
      /**
       * @param {number} r
       * @param {number} v
       * @param {number} i
       * @param {!IArrayLike<number>} arr
       * @return {number}
       * @this {?}
       */
      testFn: function(r, v, i, arr) {
        assertEquals('number', typeof i);
        assertEquals(a, arr);
        const l = this.last;
        this.last = r + v;
        return this.last + l;
      },
    };

    assertEquals(10, googArray.reduce(a, scope.testFn, 0, scope));
  },

  testArrayReduceOmitDeleted() {
    const a = [0, 1, 2, 3];
    delete a[1];
    delete a[3];
    assertEquals(2, googArray.reduce(a, (rval, val, i, arr) => {
      assertEquals('number', typeof i);
      assertEquals(a, arr);
      return rval + val;
    }, 0));

    /** @const */
    const scope = {
      last: 0,
      /**
       * @param {number} r
       * @param {number} v
       * @param {number} i
       * @param {!IArrayLike<number>} arr
       * @return {number}
       * @this {?}
       */
      testFn: function(r, v, i, arr) {
        assertEquals('number', typeof i);
        assertEquals(a, arr);
        const l = this.last;
        this.last = r + v;
        return this.last + l;
      },
    };

    assertEquals(2, googArray.reduce(a, scope.testFn, 0, scope));
  },

  testArrayReduceRight() {
    let a = [0, 1, 2, 3, 4];
    assertEquals('43210', googArray.reduceRight(a, (rval, val, i, arr) => {
      assertEquals('number', typeof i);
      assertEquals(a, arr);
      return rval + val;
    }, ''));

    /** @const */
    const scope = {
      last: '',
      /**
       * @param {string} r
       * @param {string} v
       * @param {number} i
       * @param {!IArrayLike<string>} arr
       * @return {string}
       * @this {?}
       */
      testFn: function(r, v, i, arr) {
        assertEquals('number', typeof i);
        assertEquals(a, arr);
        const l = this.last;
        this.last = v;
        return r + v + l;
      },
    };

    a = ['a', 'b', 'c'];
    assertEquals('_cbcab', googArray.reduceRight(a, scope.testFn, '_', scope));
  },

  testArrayReduceRightOmitsDeleted() {
    let a = [0, 1, 2, 3, 4];
    delete a[1];
    delete a[4];
    assertEquals('320', googArray.reduceRight(a, (rval, val, i, arr) => {
      assertEquals('number', typeof i);
      assertEquals(a, arr);
      return rval + val;
    }, ''));

    /** @const */
    const scope = {
      last: '',
      /**
       * @param {string} r
       * @param {string} v
       * @param {number} i
       * @param {!IArrayLike<string>} arr
       * @return {string}
       * @this {?}
       */
      testFn: function(r, v, i, arr) {
        assertEquals('number', typeof i);
        assertEquals(a, arr);
        const l = this.last;
        this.last = v;
        return r + v + l;
      },
    };

    a = ['a', 'b', 'c', 'd'];
    delete a[1];
    delete a[3];
    assertEquals('_cac', googArray.reduceRight(a, scope.testFn, '_', scope));
  },

  testArrayFind() {
    let a = [0, 1, 2, 3];
    let b = googArray.find(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 1;
    });
    assertEquals(2, b);

    b = googArray.find(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 100;
    });
    assertNull(b);

    a = 'abCD';
    b = googArray.find(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val >= 'A' && val <= 'Z';
    });
    assertEquals('C', b);

    a = 'abcd';
    b = googArray.find(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val >= 'A' && val <= 'Z';
    });
    assertNull(b);
  },

  testArrayFindOmitsDeleted() {
    const a = [0, 1, 2, 3];
    delete a[1];
    delete a[3];
    let b = googArray.find(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 1;
    });

    assertEquals(2, b);
    b = googArray.find(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 100;
    });
    assertNull(b);
  },

  testArrayFindIndex() {
    let a = [0, 1, 2, 3];
    let b = googArray.findIndex(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 1;
    });
    assertEquals(2, b);

    b = googArray.findIndex(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 100;
    });
    assertEquals(-1, b);

    a = 'abCD';
    b = googArray.findIndex(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val >= 'A' && val <= 'Z';
    });
    assertEquals(2, b);

    a = 'abcd';
    b = googArray.findIndex(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val >= 'A' && val <= 'Z';
    });
    assertEquals(-1, b);
  },

  testArrayFindIndexOmitsDeleted() {
    const a = [0, 1, 2, 3];
    delete a[1];
    delete a[3];
    let b = googArray.findIndex(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 1;
    });
    assertEquals(2, b);

    b = googArray.findIndex(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 100;
    });
    assertEquals(-1, b);
  },

  testArrayFindRight() {
    const a = [0, 1, 2, 3];
    let b = googArray.findRight(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val < 3;
    });
    assertEquals(2, b);
    b = googArray.findRight(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 100;
    });
    assertNull(b);
  },

  testArrayFindRightOmitsDeleted() {
    const a = [0, 1, 2, 3];
    delete a[1];
    delete a[3];
    let b = googArray.findRight(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val < 3;
    });
    assertEquals(2, b);
    b = googArray.findRight(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 100;
    });
    assertNull(b);
  },

  testArrayFindIndexRight() {
    let a = [0, 1, 2, 3];
    let b = googArray.findIndexRight(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val < 3;
    });
    assertEquals(2, b);

    b = googArray.findIndexRight(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 100;
    });
    assertEquals(-1, b);

    a = 'abCD';
    b = googArray.findIndexRight(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val >= 'a' && val <= 'z';
    });
    assertEquals(1, b);

    a = 'abcd';
    b = googArray.findIndexRight(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val >= 'A' && val <= 'Z';
    });
    assertEquals(-1, b);
  },

  testArrayFindIndexRightOmitsDeleted() {
    const a = [0, 1, 2, 3];
    delete a[1];
    delete a[3];
    let b = googArray.findIndexRight(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val < 3;
    });
    assertEquals(2, b);
    b = googArray.findIndexRight(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 100;
    });
    assertEquals(-1, b);
  },

  testArraySome() {
    const a = [0, 1, 2, 3];
    let b = googArray.some(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 1;
    });
    assertTrue(b);
    b = googArray.some(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 100;
    });
    assertFalse(b);
  },

  testArraySomeOmitsDeleted() {
    const a = [0, 1, 2, 3];
    delete a[1];
    delete a[3];
    let b = googArray.some(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('number', typeof val);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 1;
    });
    assertTrue(b);
    b = googArray.some(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('number', typeof val);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 100;
    });
    assertFalse(b);
  },

  testArrayEvery() {
    const a = [0, 1, 2, 3];
    let b = googArray.every(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val >= 0;
    });
    assertTrue(b);
    b = googArray.every(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 1;
    });
    assertFalse(b);
  },

  testArrayEveryOmitsDeleted() {
    const a = [0, 1, 2, 3];
    delete a[1];
    delete a[3];
    let b = googArray.every(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('number', typeof val);
      assertEquals('index is not a number', 'number', typeof index);
      return val >= 0;
    });
    assertTrue(b);
    b = googArray.every(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('number', typeof val);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 1;
    });
    assertFalse(b);
  },

  testArrayCount() {
    const a = [0, 1, 2, 3, 4];
    const context = {};
    assertEquals(3, googArray.count(a, function(element, index, array) {
      assertTrue(typeof (index) === 'number');
      assertEquals(a, array);
      assertEquals(context, this);
      return element % 2 == 0;
    }, context));

    delete a[2];
    assertEquals(
        'deleted element is ignored', 4, googArray.count(a, () => true));
  },

  testArrayContains() {
    const a = [0, 1, 2, 3];
    assertTrue('contain, Should contain 3', googArray.contains(a, 3));
    assertFalse('contain, Should not contain 4', googArray.contains(a, 4));

    const s = 'abcd';
    assertTrue('contain, Should contain d', googArray.contains(s, 'd'));
    assertFalse('contain, Should not contain e', googArray.contains(s, 'e'));
  },

  testArrayContainsOmitsDeleted() {
    const a = [0, 1, 2, 3];
    delete a[1];
    delete a[3];
    assertFalse(
        'should not contain undefined', googArray.contains(a, undefined));
  },

  testArrayInsert() {
    const a = [0, 1, 2, 3];

    googArray.insert(a, 4);
    assertEquals('insert, Should append 4', a[4], 4);
    googArray.insert(a, 3);
    assertEquals('insert, Should not append 3', a.length, 5);
    assertNotEquals('insert, Should not append 3', a[a.length - 1], 3);
  },

  testArrayInsertAt() {
    const a = [0, 1, 2, 3];

    googArray.insertAt(a, 4, 2);
    assertArrayEquals('insertAt, insert in middle', [0, 1, 4, 2, 3], a);
    googArray.insertAt(a, 5, 10);
    assertArrayEquals(
        'insertAt, too large value should append', [0, 1, 4, 2, 3, 5], a);
    googArray.insertAt(a, 6);
    assertArrayEquals(
        'insertAt, null/undefined value should insert at 0',
        [6, 0, 1, 4, 2, 3, 5], a);
    googArray.insertAt(a, 7, -2);
    assertArrayEquals(
        'insertAt, negative values start from end', [6, 0, 1, 4, 2, 7, 3, 5],
        a);
  },

  testArrayInsertArrayAt() {
    const a = [2, 5];
    googArray.insertArrayAt(a, [3, 4], 1);
    assertArrayEquals('insertArrayAt, insert in middle', [2, 3, 4, 5], a);
    googArray.insertArrayAt(a, [0, 1], 0);
    assertArrayEquals(
        'insertArrayAt, insert at beginning', [0, 1, 2, 3, 4, 5], a);
    googArray.insertArrayAt(a, [6, 7], 6);
    assertArrayEquals(
        'insertArrayAt, insert at end', [0, 1, 2, 3, 4, 5, 6, 7], a);
    googArray.insertArrayAt(a, ['x'], 4);
    assertArrayEquals(
        'insertArrayAt, insert one element', [0, 1, 2, 3, 'x', 4, 5, 6, 7], a);
    googArray.insertArrayAt(a, [], 4);
    assertArrayEquals(
        'insertArrayAt, insert 0 elements', [0, 1, 2, 3, 'x', 4, 5, 6, 7], a);
    googArray.insertArrayAt(a, ['y', 'z']);
    assertArrayEquals(
        'insertArrayAt, undefined value should insert as 0',
        ['y', 'z', 0, 1, 2, 3, 'x', 4, 5, 6, 7], a);
    googArray.insertArrayAt(a, ['a'], /** @type {?} */ (null));
    assertArrayEquals(
        'insertArrayAt, null value should insert as 0',
        ['a', 'y', 'z', 0, 1, 2, 3, 'x', 4, 5, 6, 7], a);
    googArray.insertArrayAt(a, ['b'], 100);
    assertArrayEquals(
        'insertArrayAt, too large value should append',
        ['a', 'y', 'z', 0, 1, 2, 3, 'x', 4, 5, 6, 7, 'b'], a);
    googArray.insertArrayAt(a, ['c', 'd'], -2);
    assertArrayEquals(
        'insertArrayAt, negative values start from end',
        ['a', 'y', 'z', 0, 1, 2, 3, 'x', 4, 5, 6, 'c', 'd', 7, 'b'], a);
  },

  testArrayInsertBefore() {
    const a = ['a', 'b', 'c', 'd'];
    googArray.insertBefore(a, 'e', 'b');
    assertArrayEquals(
        'insertBefore, with existing element', ['a', 'e', 'b', 'c', 'd'], a);
    googArray.insertBefore(a, 'f', 'x');
    assertArrayEquals(
        'insertBefore, with non existing element',
        ['a', 'e', 'b', 'c', 'd', 'f'], a);
  },

  testArrayRemove() {
    const a = ['a', 'b', 'c', 'd'];
    googArray.remove(a, 'c');
    assertArrayEquals('remove, remove existing element', ['a', 'b', 'd'], a);
    googArray.remove(a, 'x');
    assertArrayEquals(
        'remove, remove non existing element', ['a', 'b', 'd'], a);
  },

  testArrayRemoveLast() {
    const a = ['c', 'a', 'b', 'c', 'd', 'a'];
    googArray.removeLast(a, 'c');
    let temp = ['c', 'a', 'b', 'd', 'a'];
    assertArrayEquals('remove, remove existing element', temp, a);
    googArray.removeLast(a, 'a');
    temp = ['c', 'a', 'b', 'd'];
    assertArrayEquals('remove, remove existing element', temp, a);
    googArray.removeLast(a, 'y');
    temp = ['c', 'a', 'b', 'd'];
    assertArrayEquals('remove, remove non existing element', temp, a);
  },

  testArrayRemoveAt() {
    let a = [0, 1, 2, 3];
    googArray.removeAt(a, 2);
    assertArrayEquals('removeAt, remove existing index', [0, 1, 3], a);
    a = [0, 1, 2, 3];
    googArray.removeAt(a, 10);
    assertArrayEquals('removeAt, remove non existing index', [0, 1, 2, 3], a);
    a = [0, 1, 2, 3];
    googArray.removeAt(a, -2);
    assertArrayEquals('removeAt, remove with negative index', [0, 1, 3], a);
  },

  testArrayRemoveIf() {
    let a = [0, 1, 2, 3];
    googArray.removeIf(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 1;
    });
    assertArrayEquals('removeIf, remove existing element', [0, 1, 3], a);

    a = [0, 1, 2, 3];
    googArray.removeIf(a, (val, index, a2) => {
      assertEquals(a, a2);
      assertEquals('index is not a number', 'number', typeof index);
      return val > 100;
    });
    assertArrayEquals('removeIf, remove non-existing element', [0, 1, 2, 3], a);
  },

  testArrayClone() {
    const a = [0, 1, 2, 3];
    const a2 = googArray.clone(a);
    assertArrayEquals('clone, should be equal', a, a2);

    const b = {0: 0, 1: 1, 2: 2, 3: 3, length: 4};
    const b2 = googArray.clone(b);
    for (let i = 0; i < b.length; i++) {
      assertEquals('clone, should be equal', b[i], b2[i]);
    }
  },

  testToArray() {
    const a = [0, 1, 2, 3];
    const a2 = googArray.toArray(a);
    assertArrayEquals('toArray, should be equal', a, a2);

    const b = {0: 0, 1: 1, 2: 2, 3: 3, length: 4};
    const b2 = googArray.toArray(b);
    for (let i = 0; i < b.length; i++) {
      assertEquals('toArray, should be equal', b[i], b2[i]);
    }
  },

  testToArrayOnNonArrayLike() {
    const nonArrayLike = {};
    assertArrayEquals(
        'toArray on non ArrayLike should return an empty array', [],
        googArray.toArray(/** @type {?} */ (nonArrayLike)));

    const nonArrayLike2 = {length: 'hello world'};
    assertArrayEquals(
        'toArray on non ArrayLike should return an empty array', [],
        googArray.toArray(/** @type {?} */ (nonArrayLike2)));
  },

  testExtend() {
    let a = [0, 1];
    googArray.extend(a, [2, 3]);
    let a2 = [0, 1, 2, 3];
    assertArrayEquals('extend, should be equal', a, a2);

    let b = [0, 1];
    googArray.extend(b, 2);
    let b2 = [0, 1, 2];
    assertArrayEquals('extend, should be equal', b, b2);

    a = [0, 1];
    googArray.extend(a, [2, 3], [4, 5]);
    a2 = [0, 1, 2, 3, 4, 5];
    assertArrayEquals('extend, should be equal', a, a2);

    b = [0, 1];
    googArray.extend(b, 2, 3);
    b2 = [0, 1, 2, 3];
    assertArrayEquals('extend, should be equal', b, b2);

    const c = [0, 1];
    googArray.extend(c, 2, [3, 4], 5, [6]);
    const c2 = [0, 1, 2, 3, 4, 5, 6];
    assertArrayEquals('extend, should be equal', c, c2);

    const d = [0, 1];
    const arrayLikeObject = {0: 2, 1: 3, length: 2};
    googArray.extend(d, arrayLikeObject);
    const d2 = [0, 1, 2, 3];
    assertArrayEquals('extend, should be equal', d, d2);

    const e = [0, 1];
    const emptyArrayLikeObject = {length: 0};
    googArray.extend(e, emptyArrayLikeObject);
    assertArrayEquals('extend, should be equal', e, e);

    const f = [0, 1];
    const length3ArrayLikeObject = {0: 2, 1: 4, 2: 8, length: 3};
    googArray.extend(f, length3ArrayLikeObject, length3ArrayLikeObject);
    const f2 = [0, 1, 2, 4, 8, 2, 4, 8];
    assertArrayEquals('extend, should be equal', f2, f);

    const result = [];
    // Remeber to check for flakey timeouts if increased. Particularly on IE.
    let i = 100000;
    const bigArray = Array(i);
    while (i--) {
      bigArray[i] = i;
    }
    googArray.extend(result, bigArray);
    assertArrayEquals(bigArray, result);
  },

  testExtendWithArguments() {
    function f(var_args) {
      return arguments;
    }
    const a = [0];
    const a2 = [0, 1, 2, 3, 4, 5];
    googArray.extend(a, f(1, 2, 3), f(4, 5));
    assertArrayEquals('extend, should be equal', a, a2);
  },

  testExtendWithQuerySelector() {
    const a = [0];
    const d = dom.getElementsByTagNameAndClass(TagName.DIV, 'foo');
    googArray.extend(a, d);
    assertEquals(2, a.length);
  },

  testArraySplice() {
    const a = [0, 1, 2, 3];
    googArray.splice(a, 1, 0, 4);
    assertArrayEquals([0, 4, 1, 2, 3], a);
    googArray.splice(a, 1, 1, 5);
    assertArrayEquals([0, 5, 1, 2, 3], a);
    googArray.splice(a, 1, 1);
    assertArrayEquals([0, 1, 2, 3], a);
    // var args
    googArray.splice(a, 1, 1, 4, 5, 6);
    assertArrayEquals([0, 4, 5, 6, 2, 3], a);
  },

  testArraySlice() {
    let a = [0, 1, 2, 3];
    a = googArray.slice(a, 1, 3);
    assertArrayEquals([1, 2], a);
    a = [0, 1, 2, 3];
    a = googArray.slice(a, 1, 6);
    assertArrayEquals('slice, with too large end', [1, 2, 3], a);
    a = [0, 1, 2, 3];
    a = googArray.slice(a, 1, -1);
    assertArrayEquals('slice, with negative end', [1, 2], a);
    a = [0, 1, 2, 3];
    a = googArray.slice(a, -2, 3);
    assertArrayEquals('slice, with negative start', [2], a);
  },

  testRemoveDuplicates() {
    assertRemovedDuplicates([1, 2, 3, 4, 5, 6], [1, 2, 3, 4, 5, 6]);
    assertRemovedDuplicates(
        [9, 4, 2, 1, 3, 6, 0, -9], [9, 4, 2, 4, 4, 2, 9, 1, 3, 6, 0, -9]);
    assertRemovedDuplicates(
        ['four', 'one', 'two', 'three', 'THREE'],
        ['four', 'one', 'two', 'one', 'three', 'THREE', 'four', 'two']);
    assertRemovedDuplicates([], []);
    assertRemovedDuplicates(
        ['abc', 'hasOwnProperty', 'toString'],
        ['abc', 'hasOwnProperty', 'toString', 'abc']);

    const o1 = {};
    const o2 = {};
    const o3 = {};
    const o4 = {};
    assertRemovedDuplicates([o1, o2, o3, o4], [o1, o1, o2, o3, o2, o4]);

    // Mixed object types.
    assertRemovedDuplicates([1, '1', 2, '2'], [1, '1', 2, '2']);
    assertRemovedDuplicates(
        [true, 'true', false, 'false'], [true, 'true', false, 'false']);
    assertRemovedDuplicates(['foo'], [String('foo'), 'foo']);
    assertRemovedDuplicates([12], [Number(12), 12]);

    const obj = {};
    const uid = goog.getUid(obj);
    assertRemovedDuplicates([obj, uid], [obj, uid]);
  },

  testRemoveDuplicates_customHashFn() {
    const object1 = {key: 'foo'};
    const object2 = {key: 'bar'};
    const dupeObject = {key: 'foo'};
    const array = [object1, object2, dupeObject, 'bar'];
    const hashFn = (object) =>
        goog.isObject(object) ? object.key : (typeof object).charAt(0) + object;
    googArray.removeDuplicates(array, /* opt_rv */ undefined, hashFn);
    assertArrayEquals([object1, object2, 'bar'], array);
  },

  testBinaryInsertRemove() {
    const makeChecker = (array, fn, opt_compareFn) =>
        (value, expectResult, expectArray) => {
          const result = fn(array, value, opt_compareFn);
          assertEquals(expectResult, result);
          assertArrayEquals(expectArray, array);
        };

    const a = [];
    let check = makeChecker(a, googArray.binaryInsert);
    check(3, true, [3]);
    check(3, false, [3]);
    check(1, true, [1, 3]);
    check(5, true, [1, 3, 5]);
    check(2, true, [1, 2, 3, 5]);
    check(2, false, [1, 2, 3, 5]);

    check = makeChecker(a, googArray.binaryRemove);
    check(0, false, [1, 2, 3, 5]);
    check(3, true, [1, 2, 5]);
    check(1, true, [2, 5]);
    check(5, true, [2]);
    check(2, true, []);
    check(2, false, []);

    // test with custom comparison function, which reverse orders numbers
    const revNumCompare = (a, b) => b - a;

    check = makeChecker(a, googArray.binaryInsert, revNumCompare);
    check(3, true, [3]);
    check(3, false, [3]);
    check(1, true, [3, 1]);
    check(5, true, [5, 3, 1]);
    check(2, true, [5, 3, 2, 1]);
    check(2, false, [5, 3, 2, 1]);

    check = makeChecker(a, googArray.binaryRemove, revNumCompare);
    check(0, false, [5, 3, 2, 1]);
    check(3, true, [5, 2, 1]);
    check(1, true, [5, 2]);
    check(5, true, [2]);
    check(2, true, []);
    check(2, false, []);
  },

  testBinarySearch() {
    const insertionPoint = (position) => -(position + 1);
    let pos;

    // test default comparison on array of String(s)
    const a = [
      '1000',   '9',   'AB',   'ABC', 'ABCABC', 'ABD', 'ABDA', 'B',
      'B',      'B',   'C',    'CA',  'CC',     'ZZZ', 'ab',   'abc',
      'abcabc', 'abd', 'abda', 'b',   'c',      'ca',  'cc',   'zzz',
    ];

    assertEquals(
        '\'1000\' should be found at index 0', 0,
        googArray.binarySearch(a, '1000'));
    assertEquals(
        '\'zzz\' should be found at index ' + (a.length - 1), a.length - 1,
        googArray.binarySearch(a, 'zzz'));
    assertEquals(
        '\'C\' should be found at index 10', 10,
        googArray.binarySearch(a, 'C'));
    assertEquals(
        '\'B\' should be found at index 7', 7, googArray.binarySearch(a, 'B'));
    pos = googArray.binarySearch(a, '100');
    assertTrue('\'100\' should not be found', pos < 0);
    assertEquals(
        '\'100\' should have an insertion point of 0', 0, insertionPoint(pos));
    pos = googArray.binarySearch(a, 'zzz0');
    assertTrue('\'zzz0\' should not be found', pos < 0);
    assertEquals(
        '\'zzz0\' should have an insertion point of ' + (a.length), a.length,
        insertionPoint(pos));
    pos = googArray.binarySearch(a, 'BA');
    assertTrue('\'BA\' should not be found', pos < 0);
    assertEquals(
        '\'BA\' should have an insertion point of 10', 10, insertionPoint(pos));

    // test 0 length array with default comparison
    const b = [];

    pos = googArray.binarySearch(b, 'a');
    assertTrue('\'a\' should not be found', pos < 0);
    assertEquals(
        '\'a\' should have an insertion point of 0', 0, insertionPoint(pos));

    // test single element array with default lexiographical comparison
    const c = ['only item'];

    assertEquals(
        '\'only item\' should be found at index 0', 0,
        googArray.binarySearch(c, 'only item'));
    pos = googArray.binarySearch(c, 'a');
    assertTrue('\'a\' should not be found', pos < 0);
    assertEquals(
        '\'a\' should have an insertion point of 0', 0, insertionPoint(pos));
    pos = googArray.binarySearch(c, 'z');
    assertTrue('\'z\' should not be found', pos < 0);
    assertEquals(
        '\'z\' should have an insertion point of 1', 1, insertionPoint(pos));

    // test default comparison on array of Number(s)
    const d = [
      -897123.9,
      -321434.58758,
      -1321.3124,
      -324,
      -9,
      -3,
      0,
      0,
      0,
      0.31255,
      5,
      142.88888708,
      334,
      342,
      453,
      54254,
    ];

    assertEquals(
        '-897123.9 should be found at index 0', 0,
        googArray.binarySearch(d, -897123.9));
    assertEquals(
        '54254 should be found at index ' + (a.length - 1), d.length - 1,
        googArray.binarySearch(d, 54254));
    assertEquals(
        '-3 should be found at index 5', 5, googArray.binarySearch(d, -3));
    assertEquals(
        '0 should be found at index 6', 6, googArray.binarySearch(d, 0));
    pos = googArray.binarySearch(d, -900000);
    assertTrue('-900000 should not be found', pos < 0);
    assertEquals(
        '-900000 should have an insertion point of 0', 0, insertionPoint(pos));
    pos = googArray.binarySearch(d, 54255);
    assertTrue('54255 should not be found', pos < 0);
    assertEquals(
        '54255 should have an insertion point of ' + (d.length), d.length,
        insertionPoint(pos));
    pos = googArray.binarySearch(d, 1.1);
    assertTrue('1.1 should not be found', pos < 0);
    assertEquals(
        '1.1 should have an insertion point of 10', 10, insertionPoint(pos));

    // test with custom comparison function, which reverse orders numbers
    const revNumCompare = (a, b) => b - a;

    const e = [
      54254,
      453,
      342,
      334,
      142.88888708,
      5,
      0.31255,
      0,
      0,
      0,
      -3,
      -9,
      -324,
      -1321.3124,
      -321434.58758,
      -897123.9,
    ];

    assertEquals(
        '54254 should be found at index 0', 0,
        googArray.binarySearch(e, 54254, revNumCompare));
    assertEquals(
        '-897123.9 should be found at index ' + (e.length - 1), e.length - 1,
        googArray.binarySearch(e, -897123.9, revNumCompare));
    assertEquals(
        '-3 should be found at index 10', 10,
        googArray.binarySearch(e, -3, revNumCompare));
    assertEquals(
        '0 should be found at index 7', 7,
        googArray.binarySearch(e, 0, revNumCompare));
    pos = googArray.binarySearch(e, 54254.1, revNumCompare);
    assertTrue('54254.1 should not be found', pos < 0);
    assertEquals(
        '54254.1 should have an insertion point of 0', 0, insertionPoint(pos));
    pos = googArray.binarySearch(e, -897124, revNumCompare);
    assertTrue('-897124 should not be found', pos < 0);
    assertEquals(
        '-897124 should have an insertion point of ' + (e.length), e.length,
        insertionPoint(pos));
    pos = googArray.binarySearch(e, 1.1, revNumCompare);
    assertTrue('1.1 should not be found', pos < 0);
    assertEquals(
        '1.1 should have an insertion point of 6', 6, insertionPoint(pos));

    // test 0 length array with custom comparison function
    const f = [];

    pos = googArray.binarySearch(f, 0, revNumCompare);
    assertTrue('0 should not be found', pos < 0);
    assertEquals(
        '0 should have an insertion point of 0', 0, insertionPoint(pos));

    // test single element array with custom comparison function
    const g = [1];

    assertEquals(
        '1 should be found at index 0', 0,
        googArray.binarySearch(g, 1, revNumCompare));
    pos = googArray.binarySearch(g, 2, revNumCompare);
    assertTrue('2 should not be found', pos < 0);
    assertEquals(
        '2 should have an insertion point of 0', 0, insertionPoint(pos));
    pos = googArray.binarySearch(g, 0, revNumCompare);
    assertTrue('0 should not be found', pos < 0);
    assertEquals(
        '0 should have an insertion point of 1', 1, insertionPoint(pos));

    // test left-most duplicated element is found
    assertEquals(
        'binarySearch should find the index of the first 0', 0,
        googArray.binarySearch([0, 0, 1], 0));
    assertEquals(
        'binarySearch should find the index of the first 1', 1,
        googArray.binarySearch([0, 1, 1], 1));
  },

  testBinarySearchMaximumSizeArray() {
    const maxLength = 2 ** 32 - 1;
    // [1, empty × 4294967293, 2]
    const /** !IArrayLike<number|undefined> */ giantSparseArray = {
      length: maxLength,
      0: 1,
      [maxLength - 1]: 2,
    };
    const undefCmp = (a, b) => (a || 1.5) - (b || 1.5);

    // test with array-like object of maximum array length (2^32-1).
    assertEquals(
        '1 should be found at the start.', 0,
        googArray.binarySearch(giantSparseArray, 1, undefCmp));
    assertEquals(
        '0.5 should require insertion at the start.', -1,
        googArray.binarySearch(giantSparseArray, 0.5, undefCmp));
    assertEquals(
        '2 should be found at the end.', maxLength - 1,
        googArray.binarySearch(giantSparseArray, 2, undefCmp));
    assertEquals(
        '2.5 should require insertion at the end.', -maxLength - 1,
        googArray.binarySearch(giantSparseArray, 2.5, undefCmp));
  },

  testBinarySearchPerformance() {
    // Ensure that Array#slice, Function#apply and Function#call are not called
    // from within binarySearch, since they have performance implications in IE.

    const propertyReplacer = new PropertyReplacer();
    propertyReplacer.replace(Array.prototype, 'slice', () => {
      fail('Should not call Array#slice from binary search.');
    });
    propertyReplacer.replace(Function.prototype, 'apply', () => {
      fail('Should not call Function#apply from binary search.');
    });
    propertyReplacer.replace(Function.prototype, 'call', () => {
      fail('Should not call Function#call from binary search.');
    });

    try {
      const array =
          [1, 5, 7, 11, 13, 16, 19, 24, 28, 31, 33, 36, 40, 50, 52, 55];
      // Test with the default comparison function.
      googArray.binarySearch(array, 48);
      // Test with a custom comparison function.
      googArray.binarySearch(array, 13, (a, b) => a > b ? 1 : a < b ? -1 : 0);
    } finally {
      // The test runner uses Function.prototype.apply to call tearDown in the
      // global context so it has to be reset here.
      propertyReplacer.reset();
    }
  },

  testBinarySelect() {
    const insertionPoint = (position) => -(position + 1);
    const numbers = [
      -897123.9,
      -321434.58758,
      -1321.3124,
      -324,
      -9,
      -3,
      0,
      0,
      0,
      0.31255,
      5,
      142.88888708,
      334,
      342,
      453,
      54254,
    ];
    const objects = googArray.map(numbers, (n) => ({n: n}));
    function makeEvaluator(target) {
      return (obj, i, arr) => {
        assertEquals(objects, arr);
        assertEquals(obj, arr[i]);
        return target - obj.n;
      };
    }
    assertEquals(
        '{n:-897123.9} should be found at index 0', 0,
        googArray.binarySelect(objects, makeEvaluator(-897123.9)));
    assertEquals(
        '{n:54254} should be found at index ' + (objects.length - 1),
        objects.length - 1,
        googArray.binarySelect(objects, makeEvaluator(54254)));
    assertEquals(
        '{n:-3} should be found at index 5', 5,
        googArray.binarySelect(objects, makeEvaluator(-3)));
    assertEquals(
        '{n:0} should be found at index 6', 6,
        googArray.binarySelect(objects, makeEvaluator(0)));
    let pos = googArray.binarySelect(objects, makeEvaluator(-900000));
    assertTrue('{n:-900000} should not be found', pos < 0);
    assertEquals(
        '{n:-900000} should have an insertion point of 0', 0,
        insertionPoint(pos));
    pos = googArray.binarySelect(objects, makeEvaluator('54255'));
    assertTrue('{n:54255} should not be found', pos < 0);
    assertEquals(
        '{n:54255} should have an insertion point of ' + (objects.length),
        objects.length, insertionPoint(pos));
    pos = googArray.binarySelect(objects, makeEvaluator(1.1));
    assertTrue('{n:1.1} should not be found', pos < 0);
    assertEquals(
        '{n:1.1} should have an insertion point of 10', 10,
        insertionPoint(pos));
  },

  testArrayEquals() {
    // Test argument types.
    assertFalse('array == not array', googArray.equals([], null));
    assertFalse('not array == array', googArray.equals(null, []));
    assertFalse('not array == not array', googArray.equals(null, null));

    // Test with default comparison function.
    assertTrue('[] == []', googArray.equals([], []));
    assertTrue('[1] == [1]', googArray.equals([1], [1]));
    assertTrue('["1"] == ["1"]', googArray.equals(['1'], ['1']));
    assertFalse('[1] == ["1"]', googArray.equals([1], ['1']));
    assertTrue('[null] == [null]', googArray.equals([null], [null]));
    assertFalse('[null] == [undefined]', googArray.equals([null], [undefined]));
    assertTrue('[1, 2] == [1, 2]', googArray.equals([1, 2], [1, 2]));
    assertFalse('[1, 2] == [2, 1]', googArray.equals([1, 2], [2, 1]));
    assertFalse('[1, 2] == [1]', googArray.equals([1, 2], [1]));
    assertFalse('[1] == [1, 2]', googArray.equals([1], [1, 2]));
    assertFalse('[{}] == [{}]', googArray.equals([{}], [{}]));

    // Test with custom comparison function.
    const cmp = (a, b) => typeof a == typeof b;
    assertTrue('[] cmp []', googArray.equals([], [], cmp));
    assertTrue('[1] cmp [1]', googArray.equals([1], [1], cmp));
    assertTrue('[1] cmp [2]', googArray.equals([1], [2], cmp));
    assertTrue('["1"] cmp ["1"]', googArray.equals(['1'], ['1'], cmp));
    assertTrue('["1"] cmp ["2"]', googArray.equals(['1'], ['2'], cmp));
    assertFalse('[1] cmp ["1"]', googArray.equals([1], ['1'], cmp));
    assertTrue('[1, 2] cmp [3, 4]', googArray.equals([1, 2], [3, 4], cmp));
    assertFalse('[1] cmp [2, 3]', googArray.equals([1], [2, 3], cmp));
    assertTrue('[{}] cmp [{}]', googArray.equals([{}], [{}], cmp));
    assertTrue('[{}] cmp [{a: 1}]', googArray.equals([{}], [{a: 1}], cmp));

    // Test with array-like objects.
    assertTrue('[5] == obj [5]', googArray.equals([5], {0: 5, length: 1}));
    assertTrue('obj [5] == [5]', googArray.equals({0: 5, length: 1}, [5]));
    assertTrue(
        '["x"] == obj ["x"]', googArray.equals(['x'], {0: 'x', length: 1}));
    assertTrue(
        'obj ["x"] == ["x"]', googArray.equals({0: 'x', length: 1}, ['x']));
    assertTrue(
        '[5] == {0: 5, 1: 6, length: 1}',
        googArray.equals([5], {0: 5, 1: 6, length: 1}));
    assertTrue(
        '{0: 5, 1: 6, length: 1} == [5]',
        googArray.equals({0: 5, 1: 6, length: 1}, [5]));
    assertFalse(
        '[5, 6] == {0: 5, 1: 6, length: 1}',
        googArray.equals([5, 6], {0: 5, 1: 6, length: 1}));
    assertFalse(
        '{0: 5, 1: 6, length: 1}, [5, 6]',
        googArray.equals({0: 5, 1: 6, length: 1}, [5, 6]));
    assertTrue(
        '[5, 6] == obj [5, 6]',
        googArray.equals([5, 6], {0: 5, 1: 6, length: 2}));
    assertTrue(
        'obj [5, 6] == [5, 6]',
        googArray.equals({0: 5, 1: 6, length: 2}, [5, 6]));
    assertFalse(
        '{0: 5, 1: 6} == [5, 6]',
        googArray.equals(/** @type {?} */ ({0: 5, 1: 6}), [5, 6]));
  },

  testArrayCompare3Basic() {
    assertEquals(0, googArray.compare3([], []));
    assertEquals(0, googArray.compare3(['111', '222'], ['111', '222']));
    assertEquals(-1, googArray.compare3(['111', '222'], ['1111', '']));
    assertEquals(1, googArray.compare3(['111', '222'], ['111']));
    assertEquals(1, googArray.compare3(['11', '222', '333'], []));
    assertEquals(-1, googArray.compare3([], ['11', '222', '333']));
  },

  testArrayCompare3ComparatorFn() {
    function cmp(a, b) {
      return a - b;
    }
    assertEquals(0, googArray.compare3([], [], cmp));
    assertEquals(0, googArray.compare3([8, 4], [8, 4], cmp));
    assertEquals(-1, googArray.compare3([4, 3], [5, 0]));
    assertEquals(1, googArray.compare3([6, 2], [6]));
    assertEquals(1, googArray.compare3([1, 2, 3], []));
    assertEquals(-1, googArray.compare3([], [1, 2, 3]));
  },

  testSort() {
    // Test sorting empty array
    const a = [];
    googArray.sort(a);
    assertEquals(
        'Sorted empty array is still an empty array (length 0)', 0, a.length);

    // Test sorting homogenous array of String(s) of length > 1
    const b = [
      'JUST',
      '1',
      'test',
      'Array',
      'to',
      'test',
      'array',
      'Sort',
      'about',
      'NOW',
      '!!',
    ];
    const bSorted = [
      '!!',
      '1',
      'Array',
      'JUST',
      'NOW',
      'Sort',
      'about',
      'array',
      'test',
      'test',
      'to',
    ];
    googArray.sort(b);
    assertArrayEquals(bSorted, b);

    // Test sorting already sorted array of String(s) of length > 1
    googArray.sort(b);
    assertArrayEquals(bSorted, b);

    // Test sorting homogenous array of integer Number(s) of length > 1
    const c = [
      100,
      1,
      2000,
      -1,
      0,
      1000023,
      12312512,
      -12331,
      123,
      54325,
      -38104783,
      93708,
      908,
      -213,
      -4,
      5423,
      0,
    ];
    const cSorted = [
      -38104783,
      -12331,
      -213,
      -4,
      -1,
      0,
      0,
      1,
      100,
      123,
      908,
      2000,
      5423,
      54325,
      93708,
      1000023,
      12312512,
    ];
    googArray.sort(c);
    assertArrayEquals(cSorted, c);

    // Test sorting already sorted array of integer Number(s) of length > 1
    googArray.sort(c);
    assertArrayEquals(cSorted, c);

    // Test sorting homogenous array of Number(s) of length > 1
    const e = [
      -1321.3124,
      0.31255,
      54254,
      0,
      142.88888708,
      -321434.58758,
      -324,
      453,
      334,
      -3,
      5,
      -9,
      342,
      -897123.9,
    ];
    const eSorted = [
      -897123.9,
      -321434.58758,
      -1321.3124,
      -324,
      -9,
      -3,
      0,
      0.31255,
      5,
      142.88888708,
      334,
      342,
      453,
      54254,
    ];
    googArray.sort(e);
    assertArrayEquals(eSorted, e);

    // Test sorting already sorted array of Number(s) of length > 1
    googArray.sort(e);
    assertArrayEquals(eSorted, e);

    // Test sorting array of Number(s) of length > 1,
    // using custom comparison function which does reverse ordering
    const f = [
      -1321.3124,
      0.31255,
      54254,
      0,
      142.88888708,
      -321434.58758,
      -324,
      453,
      334,
      -3,
      5,
      -9,
      342,
      -897123.9,
    ];
    const fSorted = [
      54254,
      453,
      342,
      334,
      142.88888708,
      5,
      0.31255,
      0,
      -3,
      -9,
      -324,
      -1321.3124,
      -321434.58758,
      -897123.9,
    ];
    googArray.sort(f, (a, b) => b - a);
    assertArrayEquals(fSorted, f);

    // Test sorting already sorted array of Number(s) of length > 1
    // using custom comparison function which does reverse ordering
    googArray.sort(f, (a, b) => b - a);
    assertArrayEquals(fSorted, f);

    // Test sorting array of custom Object(s) of length > 1 that have
    // an overridden toString
    /** @constructor @struct */
    function ComparedObject(value) {
      this.value = value;
    }

    ComparedObject.prototype.toString = function() {
      return this.value;
    };

    const co1 = new ComparedObject('a');
    const co2 = new ComparedObject('b');
    const co3 = new ComparedObject('c');
    const co4 = new ComparedObject('d');

    const g = [co3, co4, co2, co1];
    const gSorted = [co1, co2, co3, co4];
    googArray.sort(g);
    assertArrayEquals(gSorted, g);

    // Test sorting already sorted array of custom Object(s) of length > 1
    // that have an overridden toString
    googArray.sort(g);
    assertArrayEquals(gSorted, g);

    // Test sorting an array of custom Object(s) of length > 1 using
    // a custom comparison function
    const h = [co4, co2, co1, co3];
    const hSorted = [co1, co2, co3, co4];
    googArray.sort(
        h, (a, b) => a.value > b.value ? 1 : a.value < b.value ? -1 : 0);
    assertArrayEquals(hSorted, h);

    // Test sorting already sorted array of custom Object(s) of length > 1
    // using a custom comparison function
    googArray.sort(h);
    assertArrayEquals(hSorted, h);

    // Test sorting arrays of length 1
    const i = ['one'];
    const iSorted = ['one'];
    googArray.sort(i);
    assertArrayEquals(iSorted, i);

    const j = [1];
    const jSorted = [1];
    googArray.sort(j);
    assertArrayEquals(jSorted, j);

    const k = [1.1];
    const kSorted = [1.1];
    googArray.sort(k);
    assertArrayEquals(kSorted, k);

    const l = [co3];
    const lSorted = [co3];
    googArray.sort(l);
    assertArrayEquals(lSorted, l);

    const m = [co2];
    const mSorted = [co2];
    googArray.sort(
        m, (a, b) => a.value > b.value ? 1 : a.value < b.value ? -1 : 0);
    assertArrayEquals(mSorted, m);
  },

  testStableSort() {
    // Test array with custom comparison function
    const arr = [
      {key: 3, val: 'a'},
      {key: 2, val: 'b'},
      {key: 3, val: 'c'},
      {key: 4, val: 'd'},
      {key: 3, val: 'e'},
    ];
    const arrClone = googArray.clone(arr);

    function comparisonFn(obj1, obj2) {
      return obj1.key - obj2.key;
    }
    googArray.stableSort(arr, comparisonFn);
    const sortedValues = [];
    for (let i = 0; i < arr.length; i++) {
      sortedValues.push(arr[i].val);
    }
    const wantedSortedValues = ['b', 'a', 'c', 'e', 'd'];
    assertArrayEquals(wantedSortedValues, sortedValues);

    // Test array without custom comparison function
    const arr2 = [];
    for (let i = 0; i < arrClone.length; i++) {
      arr2.push({
        val: arrClone[i].val,
        toString: goog.partial((index) => arrClone[index].key, i),
      });
    }
    googArray.stableSort(arr2);
    const sortedValues2 = [];
    for (let i = 0; i < arr2.length; i++) {
      sortedValues2.push(arr2[i].val);
    }
    assertArrayEquals(wantedSortedValues, sortedValues2);
  },

  testSortByKey() {
    /** @constructor @struct */
    function Item(value) {
      this.getValue = () => value;
    }
    const keyFn = (item) => item.getValue();

    // Test without custom key comparison function
    const arr1 =
        [new Item(3), new Item(2), new Item(1), new Item(5), new Item(4)];
    googArray.sortByKey(arr1, keyFn);
    const wantedSortedValues1 = [1, 2, 3, 4, 5];
    for (let i = 0; i < arr1.length; i++) {
      assertEquals(wantedSortedValues1[i], arr1[i].getValue());
    }

    // Test with custom key comparison function
    const arr2 =
        [new Item(3), new Item(2), new Item(1), new Item(5), new Item(4)];
    function comparisonFn(key1, key2) {
      return -(key1 - key2);
    }
    googArray.sortByKey(arr2, keyFn, comparisonFn);
    const wantedSortedValues2 = [5, 4, 3, 2, 1];
    for (let i = 0; i < arr2.length; i++) {
      assertEquals(wantedSortedValues2[i], arr2[i].getValue());
    }
  },

  testArrayBucketModulus() {
    // bucket things by modulus
    const a = {};
    const b = [];

    function modFive(num) {
      return num % 5;
    }

    for (let i = 0; i < 20; i++) {
      const mod = modFive(i);
      a[mod] = a[mod] || [];
      a[mod].push(i);
      b.push(i);
    }

    const buckets = googArray.bucket(b, modFive);

    for (let i = 0; i < 5; i++) {
      // The order isn't defined, but they should be the same sorted.
      googArray.sort(a[i]);
      googArray.sort(buckets[i]);
      assertArrayEquals(a[i], buckets[i]);
    }
  },

  testArrayBucketEvenOdd() {
    const a = [1, 2, 3, 4, 5, 6, 7, 8, 9];

    // test even/odd
    function isEven(value, index, array) {
      assertEquals(value, array[index]);
      assertEquals('number', typeof index);
      assertEquals(a, array);
      return value % 2 == 0;
    }

    const b = googArray.bucket(a, isEven);

    assertArrayEquals(b[true], [2, 4, 6, 8]);
    assertArrayEquals(b[false], [1, 3, 5, 7, 9]);
  },

  testArrayBucketUsingThisObject() {
    const a = [1, 2, 3, 4, 5];

    const obj = {specialValue: 2};

    /**
     * @param {number} value
     * @param {number} index
     * @param {!IArrayLike<number>} array
     * @return {number}
     * @this {?}
     */
    function isSpecialValue(value, index, array) {
      return value == this.specialValue ? 1 : 0;
    }

    const b = googArray.bucket(a, isSpecialValue, obj);
    assertArrayEquals(b[0], [1, 3, 4, 5]);
    assertArrayEquals(b[1], [2]);
  },


  testArrayBucketToMap() {
    const a = [1, 2, 3, 4, 5, 6, 7, 8, 9];
    const evenKey = {};
    const oddKey = {};

    function isEven(value, index, array) {
      assertEquals(value, array[index]);
      assertEquals('number', typeof index);
      assertEquals(a, array);
      return (value % 2 == 0) ? evenKey : oddKey;
    }

    const map = googArray.bucketToMap(a, isEven);
    assertEquals(2, map.size);
    assertArrayEquals(map.get(evenKey), [2, 4, 6, 8]);
    assertArrayEquals(map.get(oddKey), [1, 3, 5, 7, 9]);
  },


  testArrayToObject() {
    const a = [{name: 'a'}, {name: 'b'}, {name: 'c'}, {name: 'd'}];

    function getName(value, index, array) {
      assertEquals(value, array[index]);
      assertEquals('number', typeof index);
      assertEquals(a, array);
      return value.name;
    }

    const b = googArray.toObject(a, getName);

    for (let i = 0; i < a.length; i++) {
      assertEquals(a[i], b[a[i].name]);
    }
  },

  testArrayToMap() {
    const a = [{id: 0}, {id: 1}, {id: 2}, {id: NaN}];

    function getId(value, index, array) {
      assertEquals(value, array[index]);
      assertEquals('number', typeof index);
      assertEquals(a, array);
      return value.id;
    }

    const map = googArray.toMap(a, getId);
    assertEquals(map.size, a.length);
    for (const e of a) {
      assertEquals(e, map.get(e.id));
    }
  },

  testRange() {
    assertArrayEquals([], googArray.range(0));
    assertArrayEquals([], googArray.range(5, 5, 5));
    assertArrayEquals([], googArray.range(-3, -3));
    assertArrayEquals([], googArray.range(10, undefined, -1));
    assertArrayEquals([], googArray.range(8, 0));
    assertArrayEquals([], googArray.range(-5, -10, 3));

    assertArrayEquals([0], googArray.range(1));
    assertArrayEquals([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], googArray.range(10));

    assertArrayEquals([1], googArray.range(1, 2));
    assertArrayEquals([-3, -2, -1, 0, 1, 2], googArray.range(-3, 3));

    assertArrayEquals([4], googArray.range(4, 40, 400));
    assertArrayEquals([5, 8, 11, 14], googArray.range(5, 15, 3));
    assertArrayEquals([1, -1, -3], googArray.range(1, -5, -2));
    assertElementsRoughlyEqual(
        [.2, .3, .4], googArray.range(.2, .5, .1), 0.001);

    assertArrayEquals([0], googArray.range(7, undefined, 9));
    assertArrayEquals([0, 2, 4, 6], googArray.range(8, undefined, 2));
  },

  testArrayRepeat() {
    assertArrayEquals([], googArray.repeat(3, 0));
    assertArrayEquals([], googArray.repeat(3, -1));
    assertArrayEquals([3], googArray.repeat(3, 1));
    assertArrayEquals([3, 3, 3], googArray.repeat(3, 3));
    assertArrayEquals([null, null], googArray.repeat(null, 2));
  },

  testArrayFlatten() {
    assertArrayEquals([1, 2, 3, 4, 5], googArray.flatten(1, 2, 3, 4, 5));
    assertArrayEquals([1, 2, 3, 4, 5], googArray.flatten(1, [2, [3, [4, 5]]]));
    assertArrayEquals([1, 2, 3, 4], googArray.flatten(1, [2, [3, [4]]]));
    assertArrayEquals([1, 2, 3, 4], googArray.flatten([[[1], 2], 3], 4));
    assertArrayEquals([1], googArray.flatten([[1]]));
    assertArrayEquals([], googArray.flatten());
    assertArrayEquals([], googArray.flatten([]));
    assertArrayEquals(
        googArray.repeat(3, 180002),
        googArray.flatten(3, googArray.repeat(3, 180000), 3));
    assertArrayEquals(
        googArray.repeat(3, 180000),
        googArray.flatten([googArray.repeat(3, 180000)]));
  },

  testSortObjectsByKey() {
    const sortedArray = buildSortedObjectArray(4);
    const objects =
        [sortedArray[1], sortedArray[2], sortedArray[0], sortedArray[3]];

    googArray.sortObjectsByKey(objects, 'name');
    assertArrayEquals(sortedArray, objects);
  },

  testSortObjectsByKeyWithCompareFunction() {
    const sortedArray = buildSortedObjectArray(4);
    const objects =
        [sortedArray[1], sortedArray[2], sortedArray[0], sortedArray[3]];
    const descSortedArray =
        [sortedArray[3], sortedArray[2], sortedArray[1], sortedArray[0]];

    function descCompare(a, b) {
      return a < b ? 1 : a > b ? -1 : 0;
    }

    googArray.sortObjectsByKey(objects, 'name', descCompare);
    assertArrayEquals(descSortedArray, objects);
  },

  testIsSorted() {
    assertTrue(googArray.isSorted([1, 2, 3]));
    assertTrue(googArray.isSorted([1, 2, 2]));
    assertFalse(googArray.isSorted([1, 2, 1]));

    assertTrue(googArray.isSorted([1, 2, 3], null, true));
    assertFalse(googArray.isSorted([1, 2, 2], null, true));
    assertFalse(googArray.isSorted([1, 2, 1], null, true));

    function compare(a, b) {
      return b - a;
    }

    assertFalse(googArray.isSorted([1, 2, 3], compare));
    assertTrue(googArray.isSorted([3, 2, 2], compare));
  },

  testRotate() {
    assertRotated([], [], 3);
    assertRotated([1], [1], 3);
    assertRotated([1, 2, 3, 4, 0], [0, 1, 2, 3, 4], -6);
    assertRotated([0, 1, 2, 3, 4], [0, 1, 2, 3, 4], -5);
    assertRotated([4, 0, 1, 2, 3], [0, 1, 2, 3, 4], -4);
    assertRotated([3, 4, 0, 1, 2], [0, 1, 2, 3, 4], -3);
    assertRotated([2, 3, 4, 0, 1], [0, 1, 2, 3, 4], -2);
    assertRotated([1, 2, 3, 4, 0], [0, 1, 2, 3, 4], -1);
    assertRotated([0, 1, 2, 3, 4], [0, 1, 2, 3, 4], 0);
    assertRotated([4, 0, 1, 2, 3], [0, 1, 2, 3, 4], 1);
    assertRotated([3, 4, 0, 1, 2], [0, 1, 2, 3, 4], 2);
    assertRotated([2, 3, 4, 0, 1], [0, 1, 2, 3, 4], 3);
    assertRotated([1, 2, 3, 4, 0], [0, 1, 2, 3, 4], 4);
    assertRotated([0, 1, 2, 3, 4], [0, 1, 2, 3, 4], 5);
    assertRotated([4, 0, 1, 2, 3], [0, 1, 2, 3, 4], 6);
  },

  testMoveItemWithArray() {
    const arr = [0, 1, 2, 3];
    googArray.moveItem(arr, 1, 3);  // toIndex > fromIndex
    assertArrayEquals([0, 2, 3, 1], arr);
    googArray.moveItem(arr, 2, 0);  // toIndex < fromIndex
    assertArrayEquals([3, 0, 2, 1], arr);
    googArray.moveItem(arr, 1, 1);  // toIndex == fromIndex
    assertArrayEquals([3, 0, 2, 1], arr);
    // Out-of-bounds indexes throw assertion errors.
    assertThrows(() => {
      googArray.moveItem(arr, -1, 1);
    });
    assertThrows(() => {
      googArray.moveItem(arr, 4, 1);
    });
    assertThrows(() => {
      googArray.moveItem(arr, 1, -1);
    });
    assertThrows(() => {
      googArray.moveItem(arr, 1, 4);
    });
    // The array should not be modified by the out-of-bound calls.
    assertArrayEquals([3, 0, 2, 1], arr);
  },

  testMoveItemWithArgumentsObject() {
    const f = function(var_args) {
      googArray.moveItem(arguments, 0, 1);
      return arguments;
    };
    assertArrayEquals([1, 0], googArray.toArray(f(0, 1)));
  },

  testConcat() {
    const a1 = [1, 2, 3];
    const a2 = [4, 5, 6];
    const a3 = googArray.concat(a1, a2);
    a1.push(1);
    a2.push(5);
    assertArrayEquals([1, 2, 3, 4, 5, 6], a3);
  },

  testConcatWithNoSecondArg() {
    const a1 = [1, 2, 3, 4];
    const a2 = googArray.concat(a1);
    a1.push(5);
    assertArrayEquals([1, 2, 3, 4], a2);
  },

  testConcatWithNonArrayArgs() {
    const a1 = [1, 2, 3, 4];
    const o = {0: 'a', 1: 'b', length: 2};
    const a2 = googArray.concat(a1, 5, '10', o);
    assertArrayEquals([1, 2, 3, 4, 5, '10', o], a2);
  },

  testConcatWithNull() {
    const a1 = googArray.concat(null, [1, 2, 3]);
    const a2 = googArray.concat([1, 2, 3], null);
    assertArrayEquals([null, 1, 2, 3], a1);
    assertArrayEquals([1, 2, 3, null], a2);
  },

  testZip() {
    const a1 = googArray.zip([1, 2, 3], [3, 2, 1]);
    const a2 = googArray.zip([1, 2], [3, 2, 1]);
    const a3 = googArray.zip();
    assertArrayEquals([[1, 3], [2, 2], [3, 1]], a1);
    assertArrayEquals([[1, 3], [2, 2]], a2);
    assertArrayEquals([], a3);
  },

  testShuffle() {
    // Test array. This array should have unique values for the purposes of this
    // test case.
    const testArray = [1, 2, 3, 4, 5];
    const testArrayCopy = googArray.clone(testArray);

    // Custom random function, which always returns a value approaching 1,
    // resulting in a "shuffle" that preserves the order of original array
    // (for array sizes that we work with here).
    const noChangeShuffleFunction = () => .999999;
    googArray.shuffle(testArray, noChangeShuffleFunction);
    assertArrayEquals(testArrayCopy, testArray);

    // Custom random function, which always returns 0, resulting in a
    // deterministic "shuffle" that is predictable but differs from the
    // original order of the array.
    const testShuffleFunction = () => 0;
    googArray.shuffle(testArray, testShuffleFunction);
    assertArrayEquals([2, 3, 4, 5, 1], testArray);

    // Test the use of a real random function(no optional RNG is specified).
    googArray.shuffle(testArray);

    // Ensure the shuffled array comprises the same elements (without regard to
    // order).
    assertSameElements(testArrayCopy, testArray);
  },

  testRemoveAllIf() {
    const testArray = [9, 1, 9, 2, 9, 3, 4, 9, 9, 9, 5];
    const expectedArray = [1, 2, 3, 4, 5];

    const actualOutput = googArray.removeAllIf(testArray, (el) => el == 9);

    assertEquals(6, actualOutput);
    assertArrayEquals(expectedArray, testArray);
  },

  testRemoveAllIf_noMatches() {
    const testArray = [1];
    const expectedArray = [1];

    const actualOutput = googArray.removeAllIf(testArray, (el) => false);

    assertEquals(0, actualOutput);
    assertArrayEquals(expectedArray, testArray);
  },

  testCopyByIndex() {
    const testArray = [1, 2, 'a', 'b', 'c', 'd'];
    const copyIndexes = [1, 3, 0, 0, 2];
    const expectedArray = [2, 'b', 1, 1, 'a'];

    const actualOutput = googArray.copyByIndex(testArray, copyIndexes);

    assertArrayEquals(expectedArray, actualOutput);
  },

  testComparators() {
    const greater = 42;
    const smaller = 13;

    assertTrue(googArray.defaultCompare(smaller, greater) < 0);
    assertEquals(0, googArray.defaultCompare(smaller, smaller));
    assertTrue(googArray.defaultCompare(greater, smaller) > 0);

    assertTrue(googArray.inverseDefaultCompare(greater, smaller) < 0);
    assertEquals(0, googArray.inverseDefaultCompare(greater, greater));
    assertTrue(googArray.inverseDefaultCompare(smaller, greater) > 0);
  },

  testConcatMap() {
    const a = [0, 1, 2, 0];
    const context = {};
    const arraysToReturn = [['x', 'y', 'z'], [], ['a', 'b']];
    let timesCalled = 0;
    const result = googArray.concatMap(a, function(val, index, a2) {
      assertEquals(a, a2);
      assertEquals(context, this);
      assertEquals(timesCalled++, index);
      assertEquals(a[index], val);
      return arraysToReturn[val];
    }, context);
    assertArrayEquals(['x', 'y', 'z', 'a', 'b', 'x', 'y', 'z'], result);
  },
});
