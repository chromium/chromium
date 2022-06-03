/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.iterTest');
goog.setTestOnly();

const IterIterator = goog.require('goog.iter.Iterator');
const StopIteration = goog.require('goog.iter.StopIteration');
const googIter = goog.require('goog.iter');
const testSuite = goog.require('goog.testing.testSuite');

class ArrayIterator extends IterIterator {
  constructor(array) {
    super();
    this.array_ = array;
    this.current_ = 0;
  }

  /** @override */
  nextValueOrThrow() {
    if (this.current_ >= this.array_.length) {
      throw StopIteration;
    }
    return this.array_[this.current_++];
  }
}

// Return the product of several arrays as an array
function productAsArray(var_args) {
  const iter = googIter.product.apply(null, arguments);
  return googIter.toArray(iter);
}

testSuite({
  testForEach() {
    let s = '';
    const iter = new ArrayIterator(['a', 'b', 'c', 'd']);
    googIter.forEach(iter, (val, index, iter2) => {
      assertEquals(iter, iter2);
      assertEquals('index should be undefined', 'undefined', typeof index);
      s += val;
    });
    assertEquals('abcd', s);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testJoin() {
    let iter = new ArrayIterator(['a', 'b', 'c', 'd']);
    assertEquals('abcd', googIter.join(iter, ''));

    iter = new ArrayIterator(['a', 'b', 'c', 'd']);
    assertEquals('a,b,c,d', googIter.join(iter, ','));

    // make sure everything is treated as strings
    iter = new ArrayIterator([0, 1, 2, 3]);
    assertEquals('0123', googIter.join(iter, ''));

    iter = new ArrayIterator([0, 1, 2, 3]);
    assertEquals('0919293', googIter.join(iter, 9));

    // Joining an empty iterator should result in an empty string
    iter = new ArrayIterator([]);
    assertEquals('', googIter.join(iter, ','));
  },

  testRange() {
    let iter = googIter.range(0, 5, 1);
    assertEquals('01234', googIter.join(iter, ''));

    iter = googIter.range(0, 5, 2);
    assertEquals('024', googIter.join(iter, ''));

    iter = googIter.range(0, 5, 5);
    assertEquals('0', googIter.join(iter, ''));

    iter = googIter.range(0, 5, 10);
    assertEquals('0', googIter.join(iter, ''));

    // negative step
    iter = googIter.range(5, 0, -1);
    assertEquals('54321', googIter.join(iter, ''));

    iter = googIter.range(5, 0, -2);
    assertEquals('531', googIter.join(iter, ''));

    iter = googIter.range(5, 0, -5);
    assertEquals('5', googIter.join(iter, ''));

    iter = googIter.range(5, 0, -10);
    assertEquals('5', googIter.join(iter, ''));

    // wrong direction should result in empty iterator
    iter = googIter.range(0, 5, -1);
    assertEquals('', googIter.join(iter, ''));

    iter = googIter.range(5, 0, 1);
    assertEquals('', googIter.join(iter, ''));

    // a step of 0 is not allowed
    googIter.range(0, 5, 0);

    // test the opt args
    iter = googIter.range(0, 5);
    assertEquals('01234', googIter.join(iter, ''));

    iter = googIter.range(5);
    assertEquals('01234', googIter.join(iter, ''));
  },

  testFilter() {
    let iter = googIter.range(5);
    const iter2 = googIter.filter(iter, (val, index, iter3) => {
      assertEquals(iter, iter3);
      assertEquals('index should be undefined', 'undefined', typeof index);
      return val > 1;
    });

    assertEquals('234', googIter.join(iter2, ''));

    // Chaining filters
    iter = googIter.range(10);
    const sb = [];
    const evens = googIter.filter(iter, (v) => {
      sb.push(`a${v}`);
      return v % 2 == 0;
    });
    const evens2 = googIter.filter(evens, (v) => {
      sb.push(`b${v}`);
      return v >= 5;
    });

    assertEquals('68', googIter.join(evens2, ''));
    // Note the order here. The next calls are done lazily.
    assertEquals('a0b0a1a2b2a3a4b4a5a6b6a7a8b8a9', sb.join(''));
  },

  testFilterFalse() {
    let iter = googIter.range(5);
    const iter2 = googIter.filterFalse(iter, (val, index, iter3) => {
      assertEquals(iter, iter3);
      assertEquals('index should be undefined', 'undefined', typeof index);
      return val < 2;
    });

    assertEquals('234', googIter.join(iter2, ''));

    // Chaining filters
    iter = googIter.range(10);
    const sb = [];
    const odds = googIter.filterFalse(iter, (v) => {
      sb.push(`a${v}`);
      return v % 2 == 0;
    });
    const odds2 = googIter.filterFalse(odds, (v) => {
      sb.push(`b${v}`);
      return v <= 5;
    });

    assertEquals('79', googIter.join(odds2, ''));
    // Note the order here. The next calls are done lazily.
    assertEquals('a0a1b1a2a3b3a4a5b5a6a7b7a8a9b9', sb.join(''));
  },

  testMap() {
    const iter = googIter.range(4);
    const iter2 = googIter.map(iter, (val, index, iter3) => {
      assertEquals(iter, iter3);
      assertEquals('index should be undefined', 'undefined', typeof index);
      return val * val;
    });
    assertEquals('0149', googIter.join(iter2, ''));
  },

  testReduce() {
    const iter = googIter.range(1, 5);
    assertEquals(
        10,  // 1 + 2 + 3 + 4
        googIter.reduce(iter, (val, el) => val + el, 0));
  },

  testReduce2() {
    const iter = googIter.range(1, 5);
    assertEquals(
        24,  // 4!
        googIter.reduce(iter, (val, el) => val * el, 1));
  },

  testSome() {
    let iter = googIter.range(5);
    let b = googIter.some(iter, (val, index, iter3) => {
      assertEquals(iter, iter3);
      assertEquals('index should be undefined', 'undefined', typeof index);
      return val > 1;
    });
    assertTrue(b);
    iter = googIter.range(5);
    b = googIter.some(iter, (val, index, iter3) => {
      assertEquals(iter, iter3);
      assertEquals('index should be undefined', 'undefined', typeof index);
      return val > 100;
    });
    assertFalse(b);
  },

  testEvery() {
    let iter = googIter.range(5);
    let b = googIter.every(iter, (val, index, iter3) => {
      assertEquals(iter, iter3);
      assertEquals('index should be undefined', 'undefined', typeof index);
      return val >= 0;
    });
    assertTrue(b);
    iter = googIter.range(5);
    b = googIter.every(iter, (val, index, iter3) => {
      assertEquals(iter, iter3);
      assertEquals('index should be undefined', 'undefined', typeof index);
      return val > 1;
    });
    assertFalse(b);
  },

  testChain() {
    let iter = googIter.range(0, 2);
    let iter2 = googIter.range(2, 4);
    const iter3 = googIter.range(4, 6);
    const iter4 = googIter.chain(iter, iter2, iter3);

    assertEquals('012345', googIter.join(iter4, ''));

    // empty iter
    iter = new IterIterator;
    iter2 = googIter.chain(iter);
    assertEquals('', googIter.join(iter2, ''));

    // no args
    iter2 = googIter.chain();
    assertEquals('', googIter.join(iter2, ''));

    // arrays
    const arr = [0, 1];
    const arr2 = [2, 3];
    const arr3 = [4, 5];
    iter = googIter.chain(arr, arr2, arr3);
    assertEquals('012345', googIter.join(iter, ''));
  },

  testChainFromIterable() {
    const arg = [0, 1];
    const arg2 = [2, 3];
    const arg3 = googIter.range(4, 6);
    const iter = googIter.chainFromIterable([arg, arg2, arg3]);
    assertEquals('012345', googIter.join(iter, ''));
  },

  testChainFromIterable2() {
    const arg = googIter.zip([0, 3], [1, 4], [2, 5]);
    const iter = googIter.chainFromIterable(arg);
    assertEquals('012345', googIter.join(iter, ''));
  },

  testDropWhile() {
    const iter = googIter.range(10);
    const iter2 = googIter.dropWhile(iter, (val, index, iter3) => {
      assertEquals(iter, iter3);
      assertEquals('index should be undefined', 'undefined', typeof index);
      return val < 5;
    });

    assertEquals('56789', googIter.join(iter2, ''));
  },

  testDropWhile2() {
    const iter = googIter.range(10);
    const iter2 = googIter.dropWhile(iter, (val, index, iter3) => {
      assertEquals(iter, iter3);
      assertEquals('index should be undefined', 'undefined', typeof index);
      return val != 5;
    });

    assertEquals('56789', googIter.join(iter2, ''));
  },

  testTakeWhile() {
    const iter = googIter.range(10);
    const iter2 = googIter.takeWhile(iter, (val, index, iter3) => {
      assertEquals(iter, iter3);
      assertEquals('index should be undefined', 'undefined', typeof index);
      return val < 5;
    });

    assertEquals('01234', googIter.join(iter2, ''));

    // next() should not have been called on iter after the first failure and
    // therefore it should contain some elements.  5 failed so we should have
    // the rest
    assertEquals('6789', googIter.join(iter, ''));
  },

  testTakeWhile2() {
    const iter = googIter.range(10);
    const iter2 = googIter.takeWhile(iter, (val, index, iter3) => {
      assertEquals(iter, iter3);
      assertEquals('index should be undefined', 'undefined', typeof index);
      return val != 5;
    });

    assertEquals('01234', googIter.join(iter2, ''));

    // next() should not have been called on iter after the first failure and
    // therefore it should contain some elements.  5 failed so we should have
    // the rest
    assertEquals('6789', googIter.join(iter, ''));
  },

  testToArray() {
    let iter = googIter.range(5);
    let array = googIter.toArray(iter);
    assertEquals('01234', array.join(''));

    // Empty
    iter = new IterIterator;
    array = googIter.toArray(iter);
    assertEquals('Empty iterator to array', '', array.join(''));
  },

  testToArray2() {
    let iterable = [0, 1, 2, 3, 4];
    let array = googIter.toArray(iterable);
    assertEquals('01234', array.join(''));

    // Empty
    iterable = [];
    array = googIter.toArray(iterable);
    assertEquals('Empty iterator to array', '', array.join(''));
  },

  testEquals() {
    let iter = googIter.range(5);
    let iter2 = googIter.range(5);
    assertTrue('Equal iterators', googIter.equals(iter, iter2));

    iter = googIter.range(4);
    iter2 = googIter.range(5);
    assertFalse('Second one is longer', googIter.equals(iter, iter2));

    iter = googIter.range(5);
    iter2 = googIter.range(4);
    assertFalse('First one is longer', googIter.equals(iter, iter2));

    // 2 empty iterators
    iter = new IterIterator;
    iter2 = new IterIterator;
    assertTrue('Two empty iterators are equal', googIter.equals(iter, iter2));

    iter = googIter.range(4);
    assertFalse('Same iterator', googIter.equals(iter, iter));

    // equality function
    iter = googIter.toIterator(['A', 'B', 'C']);
    iter2 = googIter.toIterator(['a', 'b', 'c']);
    const equalsFn = (a, b) => a.toLowerCase() == b.toLowerCase();
    assertTrue(
        'Case-insensitive equal', googIter.equals(iter, iter2, equalsFn));
  },

  testToIterator() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    let iter = new googIter.range(5);
    let iter2 = googIter.toIterator(iter);
    assertEquals(
        'toIterator on an iterator should return the same obejct', iter, iter2);

    const iterLikeObject = {
      next: function() {
        throw StopIteration;
      },
      nextValueOrThrow() {
        throw StopIteration;
      }
    };
    const obj = {
      __iterator__: function(opt_keys) {
        assertFalse(
            '__iterator__ should always be called with false in toIterator',
            opt_keys);
        return iterLikeObject;
      },
    };

    assertEquals(
        'Should return the return value of __iterator_(false)', iterLikeObject,
        googIter.toIterator(obj));

    // Array
    const array = [0, 1, 2, 3, 4];
    iter = googIter.toIterator(array);
    assertEquals('01234', googIter.join(iter, ''));

    // Array like
    const arrayLike = {'0': 0, '1': 1, '2': 2, length: 3};
    iter = googIter.toIterator(arrayLike);
    assertEquals('012', googIter.join(iter, ''));

    // DOM
    const dom = document.getElementById('t1').childNodes;
    iter = googIter.toIterator(dom);
    iter2 = googIter.map(iter, (el) => el.innerHTML);
    assertEquals('012', googIter.join(iter2, ''));
  },

  testNextOrValue() {
    const iter = googIter.toIterator([1]);

    assertEquals(
        'Should return value when iterator is non-empty', 1,
        googIter.nextOrValue(iter, null));
    assertNull(
        'Should return given default when iterator is empty',
        googIter.nextOrValue(iter, null));
    assertEquals(
        'Should return given default when iterator is (still) empty', -1,
        googIter.nextOrValue(iter, -1));
  },

  testProduct() {
    assertArrayEquals(
        [[1, 3], [1, 4], [2, 3], [2, 4]], productAsArray([1, 2], [3, 4]));

    assertArrayEquals(
        [
          [1, 3, 5],
          [1, 3, 6],
          [1, 4, 5],
          [1, 4, 6],
          [2, 3, 5],
          [2, 3, 6],
          [2, 4, 5],
          [2, 4, 6],
        ],
        productAsArray([1, 2], [3, 4], [5, 6]));

    assertArrayEquals([[1]], productAsArray([1]));
    assertArrayEquals([], productAsArray([1], []));
    assertArrayEquals([], productAsArray());

    const expectedResult = [];
    const a = [1, 2, 3];
    const b = [4, 5, 6];
    const c = [7, 8, 9];
    for (let i = 0; i < a.length; i++) {
      for (let j = 0; j < b.length; j++) {
        for (let k = 0; k < c.length; k++) {
          expectedResult.push([a[i], b[j], c[k]]);
        }
      }
    }

    assertArrayEquals(expectedResult, productAsArray(a, b, c));
  },

  testProductIteration() {
    let iter = googIter.product([1, 2], [3, 4]);

    assertArrayEquals([1, 3], iter.nextValueOrThrow());
    assertArrayEquals([1, 4], iter.nextValueOrThrow());
    assertArrayEquals([2, 3], iter.nextValueOrThrow());
    assertArrayEquals([2, 4], iter.nextValueOrThrow());

    let ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);

    // Ensure the iterator forever throws StopIteration.
    for (let i = 0; i < 5; i++) {
      ex = assertThrows(() => {
        iter.nextValueOrThrow();
      });
      assertEquals(StopIteration, ex);
    }

    iter = googIter.product();
    ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);

    iter = googIter.product([]);
    ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testCycle() {
    const regularArray = [1, 2, 3];
    const iter = googIter.cycle(regularArray);

    // Test 3 cycles to ensure proper cache behavior
    const values = [];
    for (let i = 0; i < 9; i++) {
      values.push(iter.nextValueOrThrow());
    }

    assertArrayEquals([1, 2, 3, 1, 2, 3, 1, 2, 3], values);
  },

  testCycleSingleItemIterable() {
    const singleItemArray = [1];

    const iter = googIter.cycle(singleItemArray);
    const values = [];

    for (let i = 0; i < 5; i++) {
      values.push(iter.nextValueOrThrow());
    }

    assertArrayEquals([1, 1, 1, 1, 1], values);
  },

  testCycleEmptyIterable() {
    const emptyArray = [];

    const iter = googIter.cycle(emptyArray);
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testCountNoArgs() {
    const iter = googIter.count();
    const values = googIter.limit(iter, 5);
    assertArrayEquals([0, 1, 2, 3, 4], googIter.toArray(values));
  },

  testCountStart() {
    const iter = googIter.count(10);
    const values = googIter.limit(iter, 5);
    assertArrayEquals([10, 11, 12, 13, 14], googIter.toArray(values));
  },

  testCountStep() {
    const iter = googIter.count(10, 2);
    const values = googIter.limit(iter, 5);
    assertArrayEquals([10, 12, 14, 16, 18], googIter.toArray(values));
  },

  testCountNegativeStep() {
    const iter = googIter.count(10, -2);
    const values = googIter.limit(iter, 5);
    assertArrayEquals([10, 8, 6, 4, 2], googIter.toArray(values));
  },

  testCountZeroStep() {
    const iter = googIter.count(42, 0);
    assertEquals(42, iter.nextValueOrThrow());
    assertEquals(42, iter.nextValueOrThrow());
    assertEquals(42, iter.nextValueOrThrow());
  },

  testCountFloat() {
    const iter = googIter.count(1.5, 0.5);
    const values = googIter.limit(iter, 5);
    assertArrayEquals([1.5, 2.0, 2.5, 3.0, 3.5], googIter.toArray(values));
  },

  testRepeat() {
    const obj = {foo: 'bar'};
    const iter = googIter.repeat(obj);
    assertEquals(obj, iter.nextValueOrThrow());
    assertEquals(obj, iter.nextValueOrThrow());
    assertEquals(obj, iter.nextValueOrThrow());
  },

  testAccumulateArray() {
    const iter = googIter.accumulate([1, 2, 3, 4, 5]);
    assertArrayEquals([1, 3, 6, 10, 15], googIter.toArray(iter));
  },

  testAccumulateIterator() {
    const iter = googIter.accumulate(googIter.range(1, 6));
    assertArrayEquals([1, 3, 6, 10, 15], googIter.toArray(iter));
  },

  testAccumulateFloat() {
    const iter = googIter.accumulate([1.0, 2.5, 0.5, 1.5, 0.5]);
    assertArrayEquals([1.0, 3.5, 4.0, 5.5, 6.0], googIter.toArray(iter));
  },

  testZipArrays() {
    const iter = googIter.zip([1, 2, 3], [4, 5, 6], [7, 8, 9]);
    assertArrayEquals([1, 4, 7], iter.nextValueOrThrow());
    assertArrayEquals([2, 5, 8], iter.nextValueOrThrow());
    assertArrayEquals([3, 6, 9], iter.nextValueOrThrow());
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testZipSingleArg() {
    const iter = googIter.zip([1, 2, 3]);
    assertArrayEquals([1], iter.nextValueOrThrow());
    assertArrayEquals([2], iter.nextValueOrThrow());
    assertArrayEquals([3], iter.nextValueOrThrow());
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testZipUnevenArgs() {
    const iter = googIter.zip([1, 2, 3], [4, 5], [7]);
    assertArrayEquals([1, 4, 7], iter.nextValueOrThrow());
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testZipNoArgs() {
    const iter = googIter.zip();
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testZipIterators() {
    const iter = googIter.zip(googIter.count(), googIter.repeat('foo'));
    assertArrayEquals([0, 'foo'], iter.nextValueOrThrow());
    assertArrayEquals([1, 'foo'], iter.nextValueOrThrow());
    assertArrayEquals([2, 'foo'], iter.nextValueOrThrow());
    assertArrayEquals([3, 'foo'], iter.nextValueOrThrow());
  },

  testZipLongestArrays() {
    const iter = googIter.zipLongest('-', 'ABCD'.split(''), 'xy'.split(''));
    assertArrayEquals(['A', 'x'], iter.nextValueOrThrow());
    assertArrayEquals(['B', 'y'], iter.nextValueOrThrow());
    assertArrayEquals(['C', '-'], iter.nextValueOrThrow());
    assertArrayEquals(['D', '-'], iter.nextValueOrThrow());
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testZipLongestSingleArg() {
    const iter = googIter.zipLongest('-', 'ABCD'.split(''));
    assertArrayEquals(['A'], iter.nextValueOrThrow());
    assertArrayEquals(['B'], iter.nextValueOrThrow());
    assertArrayEquals(['C'], iter.nextValueOrThrow());
    assertArrayEquals(['D'], iter.nextValueOrThrow());
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testZipLongestNoArgs() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const iter = googIter.zipLongest();
    assertArrayEquals([], googIter.toArray(iter));
    const iter2 = googIter.zipLongest('fill');
    assertArrayEquals([], googIter.toArray(iter2));
  },

  testZipLongestIterators() {
    const iter =
        googIter.zipLongest(null, googIter.range(3), googIter.range(5));
    assertArrayEquals([0, 0], iter.nextValueOrThrow());
    assertArrayEquals([1, 1], iter.nextValueOrThrow());
    assertArrayEquals([2, 2], iter.nextValueOrThrow());
    assertArrayEquals([null, 3], iter.nextValueOrThrow());
    assertArrayEquals([null, 4], iter.nextValueOrThrow());
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testCompressArray() {
    const iter = googIter.compress('ABCDEF'.split(''), [1, 0, 1, 0, 1, 1]);
    assertEquals('ACEF', googIter.join(iter, ''));
  },

  testCompressUnevenArgs() {
    const iter = googIter.compress('ABCDEF'.split(''), [false, true, true]);
    assertEquals('BC', googIter.join(iter, ''));
  },

  testCompressIterators() {
    const iter = googIter.compress(googIter.range(10), googIter.cycle([0, 1]));
    assertArrayEquals([1, 3, 5, 7, 9], googIter.toArray(iter));
  },

  testGroupByNoKeyFunc() {
    const iter = googIter.groupBy('AAABBBBCDD'.split(''));
    assertArrayEquals(['A', ['A', 'A', 'A']], iter.nextValueOrThrow());
    assertArrayEquals(['B', ['B', 'B', 'B', 'B']], iter.nextValueOrThrow());
    assertArrayEquals(['C', ['C']], iter.nextValueOrThrow());
    assertArrayEquals(['D', ['D', 'D']], iter.nextValueOrThrow());
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testGroupByKeyFunc() {
    const keyFunc = (x) => x.toLowerCase();
    const iter = googIter.groupBy('AaAABBbbBCccddDD'.split(''), keyFunc);
    assertArrayEquals(['a', ['A', 'a', 'A', 'A']], iter.nextValueOrThrow());
    assertArrayEquals(
        ['b', ['B', 'B', 'b', 'b', 'B']], iter.nextValueOrThrow());
    assertArrayEquals(['c', ['C', 'c', 'c']], iter.nextValueOrThrow());
    assertArrayEquals(['d', ['d', 'd', 'D', 'D']], iter.nextValueOrThrow());
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testStarMap() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const iter = googIter.starMap([[2, 5], [3, 2], [10, 3]], Math.pow);
    assertEquals(32, iter.nextValueOrThrow());
    assertEquals(9, iter.nextValueOrThrow());
    assertEquals(1000, iter.nextValueOrThrow());
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testStarMapExtraArgs() {
    const func = (string, radix, undef, iterator) => {
      assertEquals('undef should be undefined', 'undefined', typeof undef);
      assertTrue(iterator instanceof IterIterator);
      return parseInt(string, radix);
    };
    /** @suppress {checkTypes} suppression added to enable type checking */
    const iter = googIter.starMap([['42', 10], ['0xFF', 16], ['101', 2]], func);
    assertEquals(42, iter.nextValueOrThrow());
    assertEquals(255, iter.nextValueOrThrow());
    assertEquals(5, iter.nextValueOrThrow());
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testTeeArray() {
    const iters = googIter.tee('ABC'.split(''));
    assertEquals(2, iters.length);
    const it0 = iters[0];
    const it1 = iters[1];

    assertEquals('A', it0.nextValueOrThrow());
    assertEquals('A', it1.nextValueOrThrow());
    assertEquals('B', it0.nextValueOrThrow());
    assertEquals('B', it1.nextValueOrThrow());
    assertEquals('C', it0.nextValueOrThrow());
    assertEquals('C', it1.nextValueOrThrow());
    let ex = assertThrows(() => {
      it0.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
    ex = assertThrows(() => {
      it1.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testTeeIterator() {
    const iters = googIter.tee(googIter.count(), 3);
    assertEquals(3, iters.length);
    const it0 = iters[0];
    const it1 = iters[1];
    const it2 = iters[2];

    assertEquals(0, it0.nextValueOrThrow());
    assertEquals(1, it0.nextValueOrThrow());
    assertEquals(0, it1.nextValueOrThrow());
    assertEquals(1, it1.nextValueOrThrow());
    assertEquals(2, it1.nextValueOrThrow());
    assertEquals(2, it0.nextValueOrThrow());
    assertEquals(0, it2.nextValueOrThrow());
    assertEquals(1, it2.nextValueOrThrow());
    assertEquals(2, it2.nextValueOrThrow());
    assertEquals(3, it0.nextValueOrThrow());
    assertEquals(3, it1.nextValueOrThrow());
    assertEquals(3, it2.nextValueOrThrow());
  },

  testEnumerateNoStart() {
    const iter = googIter.enumerate('ABC'.split(''));
    assertArrayEquals([0, 'A'], iter.nextValueOrThrow());
    assertArrayEquals([1, 'B'], iter.nextValueOrThrow());
    assertArrayEquals([2, 'C'], iter.nextValueOrThrow());
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testEnumerateStart() {
    const iter = googIter.enumerate('DEF'.split(''), 3);
    assertArrayEquals([3, 'D'], iter.nextValueOrThrow());
    assertArrayEquals([4, 'E'], iter.nextValueOrThrow());
    assertArrayEquals([5, 'F'], iter.nextValueOrThrow());
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testLimitLess() {
    const iter = googIter.limit('ABCDEFG'.split(''), 3);
    assertEquals('ABC', googIter.join(iter, ''));
  },

  testLimitGreater() {
    const iter = googIter.limit('ABCDEFG'.split(''), 10);
    assertEquals('ABCDEFG', googIter.join(iter, ''));
  },

  testConsumeLess() {
    const iter = googIter.consume('ABCDEFG'.split(''), 3);
    assertEquals('DEFG', googIter.join(iter, ''));
  },

  testConsumeGreater() {
    const iter = googIter.consume('ABCDEFG'.split(''), 10);
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testSliceStart() {
    const iter = googIter.slice('ABCDEFG'.split(''), 2);
    assertEquals('CDEFG', googIter.join(iter, ''));
  },

  testSliceStop() {
    const iter = googIter.slice('ABCDEFG'.split(''), 2, 4);
    assertEquals('CD', googIter.join(iter, ''));
  },

  testSliceStartStopEqual() {
    const iter = googIter.slice('ABCDEFG'.split(''), 1, 1);
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testSliceIterator() {
    const iter = googIter.slice(googIter.count(20), 0, 5);
    assertArrayEquals([20, 21, 22, 23, 24], googIter.toArray(iter));
  },

  testSliceStartGreater() {
    const iter = googIter.slice('ABCDEFG'.split(''), 10);
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testPermutationsNoLength() {
    const iter = googIter.permutations(googIter.range(3));
    assertArrayEquals([0, 1, 2], iter.nextValueOrThrow());
    assertArrayEquals([0, 2, 1], iter.nextValueOrThrow());
    assertArrayEquals([1, 0, 2], iter.nextValueOrThrow());
    assertArrayEquals([1, 2, 0], iter.nextValueOrThrow());
    assertArrayEquals([2, 0, 1], iter.nextValueOrThrow());
    assertArrayEquals([2, 1, 0], iter.nextValueOrThrow());
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testPermutationsLength() {
    const iter = googIter.permutations('ABC'.split(''), 2);
    assertArrayEquals(['A', 'B'], iter.nextValueOrThrow());
    assertArrayEquals(['A', 'C'], iter.nextValueOrThrow());
    assertArrayEquals(['B', 'A'], iter.nextValueOrThrow());
    assertArrayEquals(['B', 'C'], iter.nextValueOrThrow());
    assertArrayEquals(['C', 'A'], iter.nextValueOrThrow());
    assertArrayEquals(['C', 'B'], iter.nextValueOrThrow());
  },

  testCombinations() {
    const iter = googIter.combinations(googIter.range(4), 3);
    assertArrayEquals([0, 1, 2], iter.nextValueOrThrow());
    assertArrayEquals([0, 1, 3], iter.nextValueOrThrow());
    assertArrayEquals([0, 2, 3], iter.nextValueOrThrow());
    assertArrayEquals([1, 2, 3], iter.nextValueOrThrow());
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testCombinationsWithReplacement() {
    const iter = googIter.combinationsWithReplacement('ABC'.split(''), 2);
    assertArrayEquals(['A', 'A'], iter.nextValueOrThrow());
    assertArrayEquals(['A', 'B'], iter.nextValueOrThrow());
    assertArrayEquals(['A', 'C'], iter.nextValueOrThrow());
    assertArrayEquals(['B', 'B'], iter.nextValueOrThrow());
    assertArrayEquals(['B', 'C'], iter.nextValueOrThrow());
    assertArrayEquals(['C', 'C'], iter.nextValueOrThrow());
    const ex = assertThrows(() => {
      iter.nextValueOrThrow();
    });
    assertEquals(StopIteration, ex);
  },

  testNoInfiniteRecursionWhenMigratingToNextValueOrThrow() {
    /**
     * This example class demonstrates the pattern used when migrating
     * definitions of a goog.iter.Iterator from defining #next to defining
     * #nextValueOrThrow.
     * In particular, the super call is problematic when it calls back into
     * `next` via `this`: this causes infinite recursion to occur.
     * @extends {IterIterator}
     * @template VALUE
     */
    class ExtendedIterator extends IterIterator {
      /**
       * Returns the next value of the iteration.  This will throw the object
       * {@see goog.iter.StopIteration} when the iteration passes the end.
       * @return {VALUE} Any object or value
       */
      nextValueOrThrow() {
        return super.nextValueOrThrow();
      }

      /**
       * Returns the next value of the iteration.  This will throw the object
       * {@see goog.iter.StopIteration} when the iteration passes the end.
       * @return {VALUE} Any object or value.
       * @deprecated To ease migration to the ES6 Iteration Protocol, this
       *     method is now called `nextValueOrThrow`.
       */
      next() {
        return ExtendedIterator.prototype.nextValueOrThrow.call(this);
      }
    }

    // Without fixing base GoogIterator's next method to explicitly call the
    // base implementation's nextValueOrThrow definition (instead of
    // `this.nextValueOrThrow()`), attempting to iterate over this iterator will
    // lead to infinite recursion.
    googIter.forEach(new ExtendedIterator(), () => {});
  },
});
