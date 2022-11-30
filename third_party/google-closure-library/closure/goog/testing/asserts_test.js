/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.assertsTest');
goog.setTestOnly();

const Deferred = goog.require('goog.async.Deferred');
const GoogPromise = goog.require('goog.Promise');
const IterIterator = goog.require('goog.iter.Iterator');
const StopIteration = goog.require('goog.iter.StopIteration');
const StructsMap = goog.require('goog.structs.Map');
const StructsSet = goog.require('goog.structs.Set');
const TestCase = goog.require('goog.testing.TestCase');
const asserts = goog.require('goog.testing.asserts');
const dom = goog.require('goog.dom');
const googArray = goog.require('goog.array');
const product = goog.require('goog.userAgent.product');
const testSuite = goog.require('goog.testing.testSuite');
const throwException = goog.require('goog.async.throwException');

const SUPPORTS_TYPED_ARRAY =
    typeof Uint8Array === 'function' && typeof Uint8Array.of === 'function';

const implicitlyTrue = [true, 1, -1, ' ', 'string', Infinity, new Object()];

const implicitlyFalse = [false, 0, '', null, undefined, NaN];

/**
 * Runs test suite (function) for a `Thenable` implementation covering
 * rejection.
 * @param {boolean} swallowUnhandledRejections
 * @param {function(function(function(?), function(?))): !IThenable<?>} factory
 * @suppress {strictMissingProperties} suppression added to enable type checking
 */
async function internalTestAssertRejects(swallowUnhandledRejections, factory) {
  try {
    // TODO(user): Stop the unhandled rejection handler from firing
    // rather than swallowing the errors.
    if (swallowUnhandledRejections) {
      GoogPromise.setUnhandledRejectionHandler(goog.nullFunction);
    }

    let e;
    e = await assertRejects(
        'valid IThenable constructor throws Error', factory(() => {
          throw new Error('test0');
        }));
    assertEquals('test0', e.message);

    e = await assertRejects(
        'valid IThenable constructor throws string error', factory(() => {
          throw 'test1';
        }));
    assertEquals('test1', e);

    e = await assertRejects(
        'valid IThenable rejects Error', factory((_, reject) => {
          reject(new Error('test2'));
        }));
    assertEquals('test2', e.message);

    e = await assertRejects(
        'valid IThenable rejects string error', factory((_, reject) => {
          reject('test3');
        }));
    assertEquals('test3', e);

    e = await assertRejects(
        'assertRejects should fail with a resolved thenable', (async () => {
          await assertRejects(factory((resolve) => resolve(undefined)));
          fail('should always throw.');
        })());
    assertEquals(
        'IThenable passed into assertRejects did not reject', e.message);
    // Record this as an expected assertion: go/failonunreportedasserts
    TestCase.invalidateAssertionException(/** @type {?} */ (e));
  } finally {
    // restore the default exception handler.
    GoogPromise.setUnhandledRejectionHandler(throwException);
  }
}

function stringForWindowIEHelper() {
  /**
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  window.stringForWindowIEResult = _displayStringForValue(window);
}

testSuite({
  testAssertTrue() {
    assertTrue(true);
    assertTrue('Good assertion', true);
    assertThrowsJsUnitException(() => {
      assertTrue(false);
    }, 'Call to assertTrue(boolean) with false');
    assertThrowsJsUnitException(() => {
      assertTrue('Should be true', false);
    }, 'Should be true\nCall to assertTrue(boolean) with false');
    assertThrowsJsUnitException(() => {
      assertTrue(null);
    }, 'Bad argument to assertTrue(boolean): <null>');
    assertThrowsJsUnitException(() => {
      assertTrue(undefined);
    }, 'Bad argument to assertTrue(boolean): <undefined>');
  },

  testAssertFalse() {
    assertFalse(false);
    assertFalse('Good assertion', false);
    assertThrowsJsUnitException(() => {
      assertFalse(true);
    }, 'Call to assertFalse(boolean) with true');
    assertThrowsJsUnitException(() => {
      assertFalse('Should be false', true);
    }, 'Should be false\nCall to assertFalse(boolean) with true');
    assertThrowsJsUnitException(() => {
      assertFalse(null);
    }, 'Bad argument to assertFalse(boolean): <null>');
    assertThrowsJsUnitException(() => {
      assertFalse(undefined);
    }, 'Bad argument to assertFalse(boolean): <undefined>');
  },

  testAssertEqualsWithString() {
    assertEquals('a', 'a');
    assertEquals('Good assertion', 'a', 'a');
    assertThrowsJsUnitException(() => {
      assertEquals('a', 'b');
    }, 'Expected <a> (String) but was <b> (String)');
    assertThrowsJsUnitException(() => {
      assertEquals('Bad assertion', 'a', 'b');
    }, 'Bad assertion\nExpected <a> (String) but was <b> (String)');
  },

  testAssertEqualsWithInteger() {
    assertEquals(1, 1);
    assertEquals('Good assertion', 1, 1);
    assertThrowsJsUnitException(() => {
      assertEquals(1, 2);
    }, 'Expected <1> (Number) but was <2> (Number)');
    assertThrowsJsUnitException(() => {
      assertEquals('Bad assertion', 1, 2);
    }, 'Bad assertion\nExpected <1> (Number) but was <2> (Number)');
  },

  testAssertNotEquals() {
    assertNotEquals('a', 'b');
    assertNotEquals('a', 'a', 'b');
    assertThrowsJsUnitException(() => {
      assertNotEquals('a', 'a');
    }, 'Expected not to be <a> (String)');
    assertThrowsJsUnitException(() => {
      assertNotEquals('a', 'a', 'a');
    }, 'a\nExpected not to be <a> (String)');
  },

  testAssertNull() {
    assertNull(null);
    assertNull('Good assertion', null);
    assertThrowsJsUnitException(() => {
      assertNull(true);
    }, 'Expected <null> but was <true> (Boolean)');
    assertThrowsJsUnitException(() => {
      assertNull('Should be null', false);
    }, 'Should be null\nExpected <null> but was <false> (Boolean)');
    assertThrowsJsUnitException(() => {
      assertNull(undefined);
    }, 'Expected <null> but was <undefined>');
    assertThrowsJsUnitException(() => {
      assertNull(1);
    }, 'Expected <null> but was <1> (Number)');
  },

  testAssertNullOrUndefined() {
    assertNullOrUndefined(null);
    assertNullOrUndefined(undefined);
    assertNullOrUndefined('Good assertion', null);
    assertNullOrUndefined('Good assertion', undefined);
    assertThrowsJsUnitException(() => {
      assertNullOrUndefined(true);
    }, 'Expected <null> or <undefined> but was <true> (Boolean)');
    assertThrowsJsUnitException(
        () => {
          assertNullOrUndefined('Should be null', false);
        },
        'Should be null\n' +
            'Expected <null> or <undefined> but was <false> (Boolean)');
    assertThrowsJsUnitException(() => {
      assertNullOrUndefined(0);
    }, 'Expected <null> or <undefined> but was <0> (Number)');
  },

  testAssertNotNull() {
    assertNotNull(true);
    assertNotNull('Good assertion', true);
    assertNotNull(false);
    assertNotNull(undefined);
    assertNotNull(1);
    assertNotNull('a');
    assertThrowsJsUnitException(() => {
      assertNotNull(null);
    }, 'Expected not to be <null>');
    assertThrowsJsUnitException(() => {
      assertNotNull('Should not be null', null);
    }, 'Should not be null\nExpected not to be <null>');
  },

  testAssertUndefined() {
    assertUndefined(undefined);
    assertUndefined('Good assertion', undefined);
    assertThrowsJsUnitException(() => {
      assertUndefined(true);
    }, 'Expected <undefined> but was <true> (Boolean)');
    assertThrowsJsUnitException(() => {
      assertUndefined('Should be undefined', false);
    }, 'Should be undefined\nExpected <undefined> but was <false> (Boolean)');
    assertThrowsJsUnitException(() => {
      assertUndefined(null);
    }, 'Expected <undefined> but was <null>');
    assertThrowsJsUnitException(() => {
      assertUndefined(1);
    }, 'Expected <undefined> but was <1> (Number)');
  },

  testAssertNotUndefined() {
    assertNotUndefined(true);
    assertNotUndefined('Good assertion', true);
    assertNotUndefined(false);
    assertNotUndefined(null);
    assertNotUndefined(1);
    assertNotUndefined('a');
    assertThrowsJsUnitException(() => {
      assertNotUndefined(undefined);
    }, 'Expected not to be <undefined>');
    assertThrowsJsUnitException(() => {
      assertNotUndefined('Should not be undefined', undefined);
    }, 'Should not be undefined\nExpected not to be <undefined>');
  },

  testAssertNotNullNorUndefined() {
    assertNotNullNorUndefined(true);
    assertNotNullNorUndefined('Good assertion', true);
    assertNotNullNorUndefined(false);
    assertNotNullNorUndefined(1);
    assertNotNullNorUndefined(0);
    assertNotNullNorUndefined('a');
    assertThrowsJsUnitException(() => {
      assertNotNullNorUndefined(undefined);
    }, 'Expected not to be <undefined>');
    assertThrowsJsUnitException(() => {
      assertNotNullNorUndefined('Should not be undefined', undefined);
    }, 'Should not be undefined\nExpected not to be <undefined>');
    assertThrowsJsUnitException(() => {
      assertNotNullNorUndefined(null);
    }, 'Expected not to be <null>');
    assertThrowsJsUnitException(() => {
      assertNotNullNorUndefined('Should not be null', null);
    }, 'Should not be null\nExpected not to be <null>');
  },

  testAssertNonEmptyString() {
    assertNonEmptyString('hello');
    assertNonEmptyString('Good assertion', 'hello');
    assertNonEmptyString('true');
    assertNonEmptyString('false');
    assertNonEmptyString('1');
    assertNonEmptyString('null');
    assertNonEmptyString('undefined');
    assertNonEmptyString('\n');
    assertNonEmptyString(' ');
    assertThrowsJsUnitException(() => {
      assertNonEmptyString('');
    }, 'Expected non-empty string but was <> (String)');
    assertThrowsJsUnitException(
        () => {
          assertNonEmptyString('Should be non-empty string', '');
        },
        'Should be non-empty string\n' +
            'Expected non-empty string but was <> (String)');
    assertThrowsJsUnitException(() => {
      assertNonEmptyString(true);
    }, 'Expected non-empty string but was <true> (Boolean)');
    assertThrowsJsUnitException(() => {
      assertNonEmptyString(false);
    }, 'Expected non-empty string but was <false> (Boolean)');
    assertThrowsJsUnitException(() => {
      assertNonEmptyString(1);
    }, 'Expected non-empty string but was <1> (Number)');
    assertThrowsJsUnitException(() => {
      assertNonEmptyString(null);
    }, 'Expected non-empty string but was <null>');
    assertThrowsJsUnitException(() => {
      assertNonEmptyString(undefined);
    }, 'Expected non-empty string but was <undefined>');
    assertThrowsJsUnitException(() => {
      assertNonEmptyString(['hello']);
    }, 'Expected non-empty string but was <hello> (Array)');
    // Different browsers return different values/types in the failure
    // message so don't bother checking if the message is exactly as
    // expected.
    assertThrowsJsUnitException(() => {
      assertNonEmptyString(dom.createTextNode('hello'));
    });
  },

  testAssertNaN() {
    assertNaN(NaN);
    assertNaN('Good assertion', NaN);
    assertThrowsJsUnitException(() => {
      assertNaN(1);
    }, 'Expected NaN but was <1> (Number)');
    assertThrowsJsUnitException(() => {
      assertNaN('Should be NaN', 1);
    }, 'Should be NaN\nExpected NaN but was <1> (Number)');
    assertThrowsJsUnitException(() => {
      assertNaN(true);
    }, 'Expected NaN but was <true> (Boolean)');
    assertThrowsJsUnitException(() => {
      assertNaN(false);
    }, 'Expected NaN but was <false> (Boolean)');
    assertThrowsJsUnitException(() => {
      assertNaN(null);
    }, 'Expected NaN but was <null>');
    assertThrowsJsUnitException(() => {
      assertNaN('');
    }, 'Expected NaN but was <> (String)');

    // TODO(user): These assertions fail. We should decide on the
    // semantics of assertNaN
    // assertThrowsJsUnitException(function() { assertNaN(undefined); },
    //    'Expected NaN');
    // assertThrowsJsUnitException(function() { assertNaN('a'); },
    //    'Expected NaN');
  },

  testAssertNotNaN() {
    assertNotNaN(1);
    assertNotNaN('Good assertion', 1);
    assertNotNaN(true);
    assertNotNaN(false);
    assertNotNaN('');
    assertNotNaN(null);

    // TODO(user): These assertions fail. We should decide on the
    // semantics of assertNotNaN
    // assertNotNaN(undefined);
    // assertNotNaN('a');

    assertThrowsJsUnitException(() => {
      assertNotNaN(Number.NaN);
    }, 'Expected not NaN');
    assertThrowsJsUnitException(() => {
      assertNotNaN('Should not be NaN', Number.NaN);
    }, 'Should not be NaN\nExpected not NaN');
  },

  testAssertObjectEquals() {
    const obj1 = [{'a': 'hello', 'b': 'world'}];
    const obj2 = [{'a': 'hello', 'c': 'dear', 'b': 'world'}];

    // Check with obj1 and obj2 as first and second arguments respectively.
    assertThrowsJsUnitException(() => {
      assertObjectEquals(obj1, obj2);
    });

    // Check with obj1 and obj2 as second and first arguments respectively.
    assertThrowsJsUnitException(() => {
      assertObjectEquals(obj2, obj1);
    });

    // Test if equal objects are considered equal.
    const obj3 = [{'b': 'world', 'a': 'hello'}];
    assertObjectEquals(obj1, obj3);
    assertObjectEquals(obj3, obj1);

    // Test with a case where one of the members has an undefined value.
    const obj4 = [{'a': 'hello', 'b': undefined}];
    const obj5 = [{'a': 'hello'}];

    // Check with obj4 and obj5 as first and second arguments respectively.
    assertThrowsJsUnitException(() => {
      assertObjectEquals(obj4, obj5);
    });

    // Check with obj5 and obj4 as first and second arguments respectively.
    assertThrowsJsUnitException(() => {
      assertObjectEquals(obj5, obj4);
    });

    // Check with identical Trusted Types instances.
    if (typeof window.trustedTypes !== 'undefined') {
      const policy = trustedTypes.createPolicy('testAssertObjectEquals', {
        createHTML: (s) => {
          return s;
        }
      });

      const tt1 = policy.createHTML('hello');
      const tt2 = policy.createHTML('hello');
      assertObjectEquals(tt1, tt2);
    }
  },

  testAssertObjectNotEquals() {
    const obj1 = [{'a': 'hello', 'b': 'world'}];
    const obj2 = [{'a': 'hello', 'c': 'dear', 'b': 'world'}];

    // Check with obj1 and obj2 as first and second arguments respectively.
    assertObjectNotEquals(obj1, obj2);

    // Check with obj1 and obj2 as second and first arguments respectively.
    assertObjectNotEquals(obj2, obj1);

    // Test if equal objects are considered equal.
    const obj3 = [{'b': 'world', 'a': 'hello'}];
    let error = assertThrowsJsUnitException(() => {
      assertObjectNotEquals(obj1, obj3);
    });
    assertContains('Objects should not be equal', error.message);
    error = assertThrowsJsUnitException(() => {
      assertObjectNotEquals(obj3, obj1);
    });
    assertContains('Objects should not be equal', error.message);

    // Test with a case where one of the members has an undefined value.
    const obj4 = [{'a': 'hello', 'b': undefined}];
    const obj5 = [{'a': 'hello'}];

    // Check with obj4 and obj5 as first and second arguments respectively.
    assertObjectNotEquals(obj4, obj5);

    // Check with obj5 and obj4 as first and second arguments respectively.
    assertObjectNotEquals(obj5, obj4);

    assertObjectNotEquals(new Map([['a', '1']]), new Map([['b', '1']]));
    assertObjectNotEquals(new Set(['a', 'b']), new Set(['a']));

    if (SUPPORTS_TYPED_ARRAY) {
      assertObjectNotEquals(
          new Uint32Array([1, 2, 3]), new Uint32Array([1, 4, 3]));
    }

    // Check with different Trusted Types instances.
    if (typeof window.trustedTypes !== 'undefined') {
      const policy = trustedTypes.createPolicy('testAssertObjectNotEquals', {
        createHTML: (s) => {
          return s;
        }
      });

      const tt1 = policy.createHTML('hello');
      const tt2 = policy.createHTML('world');
      assertObjectNotEquals(tt1, tt2);
    }
  },

  testAssertObjectEquals2() {
    // NOTE: (0 in [undefined]) is true on FF but false on IE.
    // (0 in {'0': undefined}) is true on both.
    // grrr.
    assertObjectEquals('arrays should be equal', [undefined], [undefined]);
    assertThrowsJsUnitException(() => {
      assertObjectEquals([undefined, undefined], [undefined]);
    });
    assertThrowsJsUnitException(() => {
      assertObjectEquals([undefined], [undefined, undefined]);
    });
  },

  testAssertObjectEquals3() {
    // Check that objects that contain identical Map objects compare
    // as equals. We can't do a negative test because on browsers that
    // implement __iterator__ we can't check the values of the iterated
    // properties.
    const obj1 = [
      {'a': 'hi', 'b': new StructsMap('hola', 'amigo', 'como', 'estas?')},
      14,
      'yes',
      true,
    ];
    const obj2 = [
      {'a': 'hi', 'b': new StructsMap('hola', 'amigo', 'como', 'estas?')},
      14,
      'yes',
      true,
    ];
    assertObjectEquals('Objects should be equal', obj1, obj2);

    const obj3 = {'a': [1, 2]};
    const obj4 = {'a': [1, 2, 3]};
    // inner arrays should not be equal
    assertThrowsJsUnitException(() => {
      assertObjectEquals(obj3, obj4);
    });
    // inner arrays should not be equal
    assertThrowsJsUnitException(() => {
      assertObjectEquals(obj4, obj3);
    });
  },

  testAssertObjectEqualsSet() {
    // verify that Sets compare equal, when run in an environment that
    // supports iterators
    const set1 = new StructsSet();
    const set2 = new StructsSet();

    set1.add('a');
    set1.add('b');
    set1.add(13);

    set2.add('a');
    set2.add('b');
    set2.add(13);

    assertObjectEquals('sets should be equal', set1, set2);

    set2.add('hey');
    assertThrowsJsUnitException(() => {
      assertObjectEquals(set1, set2);
    });
  },

  testAssertObjectEqualsMap() {
    class FooClass {}

    const map1 = new Map([
      ['foo', 'bar'],
      [1, 2],
      [FooClass, 'bar'],
    ]);
    const map2 = new Map([
      ['foo', 'bar'],
      [1, 2],
      [FooClass, 'bar'],
    ]);

    assertObjectEquals('maps should be equal', map1, map2);

    map1.set('hi', 'hey');
    assertThrowsJsUnitException(() => {
      assertObjectEquals(map1, map2);
    });
  },

  testAssertObjectEqualsTypedArrays() {
    if (!SUPPORTS_TYPED_ARRAY) return;  // not supported in IE<11

    assertObjectEquals(
        'Float32Arrays should be equal', Float32Array.of(1, 2, 3),
        Float32Array.of(1, 2, 3));
    assertObjectEquals(
        'Float64Arrays should be equal', Float64Array.of(1, 2, 3),
        Float64Array.of(1, 2, 3));
    assertObjectEquals(
        'Int8Arrays should be equal', Int8Array.of(1, 2, 3),
        Int8Array.of(1, 2, 3));
    assertObjectEquals(
        'Int16Arrays should be equal', Int16Array.of(1, 2, 3),
        Int16Array.of(1, 2, 3));
    assertObjectEquals(
        'Int32Arrays should be equal', Int32Array.of(1, 2, 3),
        Int32Array.of(1, 2, 3));
    assertObjectEquals(
        'Uint8Arrays should be equal', Uint8Array.of(1, 2, 3),
        Uint8Array.of(1, 2, 3));
    assertObjectEquals(
        'Uint8ClampedArrays should be equal', Uint8ClampedArray.of(1, 2, 3),
        Uint8ClampedArray.of(1, 2, 3));
    assertObjectEquals(
        'Uint16Arrays should be equal', Uint16Array.of(1, 2, 3),
        Uint16Array.of(1, 2, 3));
    assertObjectEquals(
        'Uint32Arrays should be equal', Uint32Array.of(1, 2, 3),
        Uint32Array.of(1, 2, 3));

    assertThrowsJsUnitException(() => {
      assertObjectNotEquals(Uint8Array.of(1, 2), Uint8Array.of(1, 2));
    });
  },

  testAssertObjectEqualsTypedArrayDifferentBacking() {
    if (!SUPPORTS_TYPED_ARRAY) return;  // not supported in IE<11

    const buf1 = new ArrayBuffer(3 * Uint16Array.BYTES_PER_ELEMENT);
    const buf2 = new ArrayBuffer(4 * Uint16Array.BYTES_PER_ELEMENT);
    const arr1 = new Uint16Array(buf1, 0, 3);
    const arr2 = new Uint16Array(buf2, Uint16Array.BYTES_PER_ELEMENT, 3);
    for (let i = 0; i < arr1.length; ++i) {
      arr1[i] = arr2[i] = i * 2 + 1;
    }
    assertObjectEquals(
        'TypedArrays with different backing buffer lengths should be equal',
        Uint16Array.of(1, 3, 5), Uint16Array.of(0, 1, 3, 5, 7).subarray(1, 4));
  },

  testAssertObjectEqualsArrayBufferContents() {
    if (!SUPPORTS_TYPED_ARRAY) return;  // not supported in IE<11
    assertObjectEquals(
        'Same ArrayBuffer contents should be equal',
        Uint16Array.of(1, 2, 3).buffer, Uint16Array.of(1, 2, 3).buffer);
  },

  testAssertObjectNotEqualsMutatedTypedArray() {
    if (!SUPPORTS_TYPED_ARRAY) return;  // not supported in IE<11

    const arr1 = Int8Array.of(2, -5, 7);
    const arr2 = Int8Array.from(arr1);
    assertObjectEquals('TypedArrays should be equal', arr1, arr2);
    ++arr1[1];
    assertObjectNotEquals('Mutated TypedArray should not be equal', arr1, arr2);
  },

  testAssertObjectNotEqualsDifferentTypedArrays() {
    if (!SUPPORTS_TYPED_ARRAY) return;  // not supported in IE<11

    assertObjectNotEquals(
        'Float32Array and Float64Array should not be equal',
        Float32Array.of(1, 2, 3), Float64Array.of(1, 2, 3));
    assertObjectNotEquals(
        'Float32Array and Int32Array should not be equal',
        Float32Array.of(1, 2, 3), Int32Array.of(1, 2, 3));
    assertObjectNotEquals(
        'Int8Array and Int16Array should not be equal', Int8Array.of(1, 2, 3),
        Int16Array.of(1, 2, 3));
    assertObjectNotEquals(
        'Int16Array and Uint16Array should not be equal',
        Int16Array.of(1, 2, 3), Uint16Array.of(1, 2, 3));
    assertObjectNotEquals(
        'Int32Array and Uint8Array should not be equal', Int8Array.of(1, 2, 3),
        Uint8Array.of(1, 2, 3));
    assertObjectNotEquals(
        'Uint8Array and Uint8ClampedArray should not be equal',
        Uint8Array.of(1, 2, 3), Uint8ClampedArray.of(1, 2, 3));

    assertThrowsJsUnitException(() => {
      assertObjectEquals(Uint8Array.of(1, 2), Uint16Array.of(1, 2));
    });
  },

  testAssertObjectBigIntTypedArrays() {
    if (typeof BigInt64Array !== 'function')
      return;  // not supported pre-ES2020

    // Check equality.
    assertObjectEquals(
        'BigInt64Arrays should be equal',
        BigInt64Array.of(BigInt(1), BigInt(2), BigInt(3)),
        BigInt64Array.of(BigInt(1), BigInt(2), BigInt(3)));
    assertObjectEquals(
        'BigUint64Arrays should be equal',
        BigUint64Array.of(BigInt(1), BigInt(2), BigInt(3)),
        BigUint64Array.of(BigInt(1), BigInt(2), BigInt(3)));

    // Check mutation.
    const arr1 = BigInt64Array.of(BigInt(2), BigInt(-5), BigInt(7));
    /** @suppress {checkTypes} suppression added to enable type checking */
    const arr2 = BigInt64Array.from(arr1);
    assertObjectEquals('BigInt64Arrays should be equal', arr1, arr2);
    ++arr1[1];
    assertObjectNotEquals(
        'Mutated BigInt64Array should not be equal', arr1, arr2);

    // Check different types are not equal.
    assertObjectNotEquals(
        'BigInt64Array and BigUint64Array should not equal',
        BigInt64Array.of(BigInt(1), BigInt(2), BigInt(3)),
        BigUint64Array.of(BigInt(1), BigInt(2), BigInt(3)));
  },

  testAssertObjectNotEqualsTypedArrayContents() {
    if (!SUPPORTS_TYPED_ARRAY) return;  // not supported in IE<11
    assertObjectNotEquals(
        'Different Uint16Array contents should not equal',
        Uint16Array.of(1, 2, 3), Uint16Array.of(1, 3, 2));
    assertObjectNotEquals(
        'Different Float32Array contents should not equal',
        Float32Array.of(1.2, 2.4, 3.8), Float32Array.of(1.2, 2.3, 3.8));

    assertThrowsJsUnitException(() => {
      assertObjectEquals(Uint8Array.of(1, 2), Uint8Array.of(3, 2));
    });
  },

  testAssertObjectNotEqualsArrayBufferContents() {
    if (!SUPPORTS_TYPED_ARRAY) return;  // not supported in IE<11
    assertObjectNotEquals(
        'Different ArrayBuffer contents should not equal',
        Uint16Array.of(1, 3, 2).buffer, Uint16Array.of(1, 2, 3).buffer);
    assertObjectNotEquals(
        'Different ArrayBuffer contents should not equal',
        Uint16Array.of(1, 2, 3, 4).buffer, Uint16Array.of(1, 2, 3).buffer);
  },

  testAssertObjectNotEqualsTypedArrayOneExtra() {
    if (!SUPPORTS_TYPED_ARRAY) return;  // not supported in IE<11
    assertObjectNotEquals(
        'Uint8ClampedArray with extra element should not equal',
        Uint8ClampedArray.of(1, 2, 3), Uint8ClampedArray.of(1, 2, 3, 4));
    assertObjectNotEquals(
        'Float32Array with extra element should not equal',
        Float32Array.of(1, 2, 3), Float32Array.of(1, 2, 3, 4));
  },

  testAssertObjectEqualsIterNoEquals() {
    // an object with an iterator but no equals() and no map_ cannot
    // be compared
    /** @constructor @struct */
    function Thing() {
      this.what = [];
    }
    Thing.prototype.add = function(n, v) {
      this.what.push(`${n}@${v}`);
    };
    Thing.prototype.get = function(n) {
      const m = new RegExp(
          `^${n}` +
              '@(.*)$',
          '');
      for (let i = 0; i < this.what.length; ++i) {
        const match = this.what[i].match(m);
        if (match) {
          return match[1];
        }
      }
      return null;
    };
    Thing.prototype.__iterator__ = function() {
      const iter = new IterIterator;
      /**
       * @suppress {strictMissingProperties} suppression added to enable
       * type checking
       */
      iter.index = 0;
      /**
       * @suppress {strictMissingProperties} suppression added to enable
       * type checking
       */
      iter.thing = this;
      iter.nextValueOrThrow = function() {
        if (this.index < this.thing.what.length) {
          return this.thing.what[this.index++].split('@')[0];
        } else {
          throw StopIteration;
        }
      };

      return iter;
    };

    const thing1 = new Thing();
    /** @suppress {checkTypes} suppression added to enable type checking */
    thing1.name = 'thing1';
    const thing2 = new Thing();
    /** @suppress {checkTypes} suppression added to enable type checking */
    thing2.name = 'thing2';
    thing1.add('red', 'fish');
    thing1.add('blue', 'fish');

    thing2.add('red', 'fish');
    thing2.add('blue', 'fish');

    assertThrowsJsUnitException(() => {
      assertObjectEquals(thing1, thing2);
    });
  },

  testAssertObjectEqualsWithDates() {
    const date = new Date(2010, 0, 1);
    const dateWithMilliseconds = new Date(2010, 0, 1, 0, 0, 0, 1);
    assertObjectEquals(new Date(2010, 0, 1), date);
    assertThrowsJsUnitException(
        goog.partial(assertObjectEquals, date, dateWithMilliseconds));
  },

  testAssertObjectEqualsSparseArrays() {
    const arr1 = [, 2, , 4];
    const arr2 = [1, 2, 3, 4, 5];

    // Sparse arrays should not be equal
    assertThrowsJsUnitException(() => {
      assertObjectEquals(arr1, arr2);
    });

    // Sparse arrays should not be equal
    assertThrowsJsUnitException(() => {
      assertObjectEquals(arr2, arr1);
    });

    let a1 = [];
    let a2 = [];
    a2[1.8] = undefined;
    // Empty slots only equal `undefined` for natural-number array keys`
    assertThrowsJsUnitException(() => {
      assertObjectEquals(a1, a2);
    });

    a1 = [];
    a2 = [];
    a2[-999] = undefined;
    // Empty slots only equal `undefined` for natural-number array keys`
    assertThrowsJsUnitException(() => {
      assertObjectEquals(a1, a2);
    });
  },

  testAssertObjectEqualsSparseArrays2() {
    // On IE6-8, the expression "1 in [4,undefined]" evaluates to false,
    // but true on other browsers. FML. This test verifies a regression
    // where IE reported that arr4 was not equal to arr1 or arr2.
    const arr1 = [1, , 3];
    const arr2 = [1, undefined, 3];
    const arr3 = googArray.clone(arr1);
    const arr4 = [];
    arr4.push(1, undefined, 3);

    // Assert that all those arrays are equivalent pairwise.
    const arrays = [arr1, arr2, arr3, arr4];
    for (let i = 0; i < arrays.length; i++) {
      for (let j = 0; j < arrays.length; j++) {
        assertArrayEquals(arrays[i], arrays[j]);
      }
    }
  },

  testAssertObjectEqualsNestedPropertyMessage() {
    assertThrowsJsUnitException(() => {
      assertObjectEquals(
          {a: 'abc', b: 4, array: [1, 2, 3, {nested: [2, 3, 4]}]},
          {a: 'bcd', b: '4', array: [1, 5, 3, {nested: [2, 3, 4, 5]}]});
    }, `Expected <[object Object]> (Object) but was <[object Object]> (Object)
   a: Expected <abc> (String) but was <bcd> (String)
   b: Expected <4> (Number) but was <4> (String)
   array[1]: Expected <2> (Number) but was <5> (Number)
   array[3].nested: Expected 3-element array but got a 4-element array`);
  },

  testAssertObjectEqualsRootDifference() {
    assertThrowsJsUnitException(() => {
      assertObjectEquals([1], [1, 2]);
    }, `Expected <1> (Array) but was <1,2> (Array)
   Expected 1-element array but got a 2-element array`);

    assertThrowsJsUnitException(() => {
      assertObjectEquals('a', 'b');
    }, 'Expected <a> (String) but was <b> (String)');

    assertThrowsJsUnitException(() => {
      assertObjectEquals([], {});
    }, 'Expected <> (Array) but was <[object Object]> (Object)');
  },

  testAssertObjectEqualsArraysWithExtraProps() {
    const arr1 = [1];
    const arr2 = [1];
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    arr2.foo = 3;

    assertThrowsJsUnitException(() => {
      assertObjectEquals(arr1, arr2);
    });

    assertThrowsJsUnitException(() => {
      assertObjectEquals(arr2, arr1);
    });
  },

  testAssertSameElementsOnArray() {
    assertSameElements([1, 2], [2, 1]);
    assertSameElements('Good assertion', [1, 2], [2, 1]);
    assertSameElements('Good assertion with duplicates', [1, 1, 2], [2, 1, 1]);
    assertThrowsJsUnitException(() => {
      assertSameElements([1, 2], [1]);
    }, 'Expected 2 elements: [1,2], got 1 elements: [1]');
    assertThrowsJsUnitException(() => {
      assertSameElements('Should match', [1, 2], [1]);
    }, 'Should match\nExpected 2 elements: [1,2], got 1 elements: [1]');
    assertThrowsJsUnitException(() => {
      assertSameElements([1, 2], [1, 3]);
    }, 'Expected [1,2], got [1,3]');
    assertThrowsJsUnitException(() => {
      assertSameElements('Should match', [1, 2], [1, 3]);
    }, 'Should match\nExpected [1,2], got [1,3]');
    assertThrowsJsUnitException(() => {
      assertSameElements([1, 1, 2], [2, 2, 1]);
    }, 'Expected [1,1,2], got [2,2,1]');
  },

  testAssertSameElementsOnArrayLike() {
    assertSameElements({0: 0, 1: 1, length: 2}, {length: 2, 1: 1, 0: 0});
    assertThrowsJsUnitException(() => {
      assertSameElements({0: 0, 1: 1, length: 2}, {0: 0, length: 1});
    }, 'Expected 2 elements: [0,1], got 1 elements: [0]');
  },

  testAssertSameElementsOnStructsSet() {
    assertSameElements({0: 0, 1: 1, length: 2}, new StructsSet([0, 1]));
    assertThrowsJsUnitException(() => {
      assertSameElements({0: 0, 1: 1, length: 2}, new StructsSet([0]));
    }, 'Expected 2 elements: [0,1], got 1 elements: [0]');
  },

  testAssertSameElementsWithBadArguments() {
    const ex = assertThrowsJsUnitException(
        /** @suppress {checkTypes} */
        () => {
          assertSameElements([], new StructsMap());
        });
    assertContains('actual', ex.toString());
    assertContains('array-like or iterable', ex.toString());
  },

  testAssertSameElementsWithIterables() {
    const s = new Set([1, 2, 3]);
    assertSameElements({0: 3, 1: 2, 2: 1, length: 3}, s);
    assertSameElements(s, {0: 3, 1: 2, 2: 1, length: 3});
    assertSameElements([], new Set());
    assertSameElements(new Set(), []);

    assertThrowsJsUnitException(
        () => assertSameElements([1, 1], new Set([1, 1])));
    assertThrowsJsUnitException(
        () => assertSameElements(new Set([1, 1]), [1, 1]));

    assertThrowsJsUnitException(
        () => assertSameElements([1, 3], new Set([1, 2])));
    assertThrowsJsUnitException(
        () => assertSameElements(new Set([1, 2]), [1, 3]));
  },

  testAssertEvaluatesToTrue() {
    assertEvaluatesToTrue(true);
    assertEvaluatesToTrue('', true);
    assertEvaluatesToTrue('Good assertion', true);
    assertThrowsJsUnitException(() => {
      assertEvaluatesToTrue(false);
    }, 'Expected to evaluate to true');
    assertThrowsJsUnitException(() => {
      assertEvaluatesToTrue('Should be true', false);
    }, 'Should be true\nExpected to evaluate to true');
    for (let i = 0; i < implicitlyTrue.length; i++) {
      assertEvaluatesToTrue(
          String('Test ' + implicitlyTrue[i] + ' [' + i + ']'),
          implicitlyTrue[i]);
    }
    for (let i = 0; i < implicitlyFalse.length; i++) {
      assertThrowsJsUnitException(() => {
        assertEvaluatesToTrue(implicitlyFalse[i]);
      }, 'Expected to evaluate to true');
    }
  },

  testAssertEvaluatesToFalse() {
    assertEvaluatesToFalse(false);
    assertEvaluatesToFalse('Good assertion', false);
    assertThrowsJsUnitException(() => {
      assertEvaluatesToFalse(true);
    }, 'Expected to evaluate to false');
    assertThrowsJsUnitException(() => {
      assertEvaluatesToFalse('Should be false', true);
    }, 'Should be false\nExpected to evaluate to false');
    for (let i = 0; i < implicitlyFalse.length; i++) {
      assertEvaluatesToFalse(
          String('Test ' + implicitlyFalse[i] + ' [' + i + ']'),
          implicitlyFalse[i]);
    }
    for (let i = 0; i < implicitlyTrue.length; i++) {
      assertThrowsJsUnitException(() => {
        assertEvaluatesToFalse(implicitlyTrue[i]);
      }, 'Expected to evaluate to false');
    }
  },

  testAssertHTMLEquals() {
    // TODO
  },

  testAssertHashEquals() {
    assertHashEquals({a: 1, b: 2}, {b: 2, a: 1});
    assertHashEquals('Good assertion', {a: 1, b: 2}, {b: 2, a: 1});
    assertHashEquals({a: undefined}, {a: undefined});
    // Missing key.
    assertThrowsJsUnitException(() => {
      assertHashEquals({a: 1, b: 2}, {a: 1});
    }, 'Expected hash had key b that was not found');
    assertThrowsJsUnitException(() => {
      assertHashEquals('Should match', {a: 1, b: 2}, {a: 1});
    }, 'Should match\nExpected hash had key b that was not found');
    assertThrowsJsUnitException(() => {
      assertHashEquals({a: undefined}, {});
    }, 'Expected hash had key a that was not found');
    // Not equal key.
    assertThrowsJsUnitException(() => {
      assertHashEquals({a: 1}, {a: 5});
    }, 'Value for key a mismatch - expected = 1, actual = 5');
    assertThrowsJsUnitException(() => {
      assertHashEquals('Should match', {a: 1}, {a: 5});
    }, 'Should match\nValue for key a mismatch - expected = 1, actual = 5');
    assertThrowsJsUnitException(() => {
      assertHashEquals({a: undefined}, {a: 1});
    }, 'Value for key a mismatch - expected = undefined, actual = 1');
    // Extra key.
    assertThrowsJsUnitException(() => {
      assertHashEquals({a: 1}, {a: 1, b: 1});
    }, 'Actual hash had key b that was not expected');
    assertThrowsJsUnitException(() => {
      assertHashEquals('Should match', {a: 1}, {a: 1, b: 1});
    }, 'Should match\nActual hash had key b that was not expected');
  },

  testAssertRoughlyEquals() {
    assertRoughlyEquals(1, 1, 0);
    assertRoughlyEquals('Good assertion', 1, 1, 0);
    assertRoughlyEquals(1, 1.1, 0.11);
    assertRoughlyEquals(1.1, 1, 0.11);
    assertThrowsJsUnitException(() => {
      assertRoughlyEquals(1, 1.1, 0.05);
    }, 'Expected 1, but got 1.1 which was more than 0.05 away');
    assertThrowsJsUnitException(() => {
      assertRoughlyEquals('Close enough', 1, 1.1, 0.05);
    }, 'Close enough\nExpected 1, but got 1.1 which was more than 0.05 away');
  },

  testAssertContainsForArrays() {
    assertContains(1, [1, 2, 3]);
    assertContains('Should contain', 1, [1, 2, 3]);
    assertThrowsJsUnitException(() => {
      assertContains(4, [1, 2, 3]);
    }, 'Expected \'1,2,3\' to contain \'4\'');
    assertThrowsJsUnitException(() => {
      assertContains('Should contain', 4, [1, 2, 3]);
    }, 'Should contain\nExpected \'1,2,3\' to contain \'4\'');
    // assertContains uses ===.
    const o = new Object();
    assertContains(o, [o, 2, 3]);
    assertThrowsJsUnitException(() => {
      assertContains(o, [1, 2, 3]);
    }, 'Expected \'1,2,3\' to contain \'[object Object]\'');
  },

  testAssertNotContainsForArrays() {
    assertNotContains(4, [1, 2, 3]);
    assertNotContains('Should not contain', 4, [1, 2, 3]);
    assertThrowsJsUnitException(() => {
      assertNotContains(1, [1, 2, 3]);
    }, 'Expected \'1,2,3\' not to contain \'1\'');
    assertThrowsJsUnitException(() => {
      assertNotContains('Should not contain', 1, [1, 2, 3]);
    }, 'Should not contain\nExpected \'1,2,3\' not to contain \'1\'');
    // assertNotContains uses ===.
    const o = new Object();
    assertNotContains({}, [o, 2, 3]);
    assertThrowsJsUnitException(() => {
      assertNotContains(o, [o, 2, 3]);
    }, 'Expected \'[object Object],2,3\' not to contain \'[object Object]\'');
  },

  testAssertContainsForStrings() {
    assertContains('ignored msg', 'abc', 'zabcd');
    assertContains('abc', 'abc');
    assertContains('', 'abc');
    assertContains('', '');
    assertThrowsJsUnitException(() => {
      assertContains('msg', 'abc', 'bcd');
    }, 'msg\nExpected \'bcd\' to contain \'abc\'');
    assertThrowsJsUnitException(() => {
      assertContains('a', '');
    }, 'Expected \'\' to contain \'a\'');
  },

  testAssertNotContainsForStrings() {
    assertNotContains('ignored msg', 'abc', 'bcd');
    assertNotContains('a', '');
    assertThrowsJsUnitException(() => {
      assertNotContains('msg', 'abc', 'zabcd');
    }, 'msg\nExpected \'zabcd\' not to contain \'abc\'');
    assertThrowsJsUnitException(() => {
      assertNotContains('abc', 'abc');
    }, 'Expected \'abc\' not to contain \'abc\'');
    assertThrowsJsUnitException(() => {
      assertNotContains('', 'abc');
    }, 'Expected \'abc\' not to contain \'\'');
  },

  /**
   * Tests `assertContains` and 'assertNotContains` with an arbitrary type
   * that has a custom `indexOf`.
   */
  testAssertContainsAndAssertNotContainsOnCustomObjectWithIndexof() {
    const valueContained = {toString: () => 'I am in'};
    const valueNotContained = {toString: () => 'I am out'};
    const container = {
      indexOf: (value) => value === valueContained ? 1234 : -1,
      toString: () => 'I am a container',
    };
    assertContains('ignored message', valueContained, container);
    assertNotContains('ignored message', valueNotContained, container);
    assertThrowsJsUnitException(() => {
      assertContains('msg', valueNotContained, container);
    }, 'msg\nExpected \'I am a container\' to contain \'I am out\'');
    assertThrowsJsUnitException(() => {
      assertNotContains('msg', valueContained, container);
    }, 'msg\nExpected \'I am a container\' not to contain \'I am in\'');
  },

  testAssertRegExp() {
    const a = 'I like turtles';
    assertRegExp(/turtles$/, a);
    assertRegExp('turtles$', a);
    assertRegExp('Expected subject to be about turtles', /turtles$/, a);
    assertRegExp('Expected subject to be about turtles', 'turtles$', a);

    const b = 'Hello';
    assertThrowsJsUnitException(() => {
      assertRegExp(/turtles$/, b);
    }, 'Expected \'Hello\' to match RegExp /turtles$/');
    assertThrowsJsUnitException(() => {
      assertRegExp('turtles$', b);
    }, 'Expected \'Hello\' to match RegExp /turtles$/');
  },

  testAssertThrows() {
    assertThrowsJsUnitException(() => {
      assertThrows(
          'assertThrows should not pass with null param',
          /** @type {?} */ (null));
    });

    assertThrowsJsUnitException(() => {
      assertThrows(
          'assertThrows should not pass with undefined param',
          /** @type {?} */ (undefined));
    });

    assertThrowsJsUnitException(() => {
      assertThrows(
          'assertThrows should not pass with number param',
          /** @type {?} */ (1));
    });

    assertThrowsJsUnitException(() => {
      assertThrows(
          'assertThrows should not pass with string param',
          /** @type {?} */ ('string'));
    });

    assertThrowsJsUnitException(() => {
      assertThrows(
          'assertThrows should not pass with object param',
          /** @type {?} */ ({}));
    });

    let error;
    try {
      error = assertThrows('valid function throws Error', () => {
        throw new Error('test');
      });
    } catch (e) {
      fail('assertThrows incorrectly doesn\'t detect a thrown exception');
    }
    assertEquals('error message', 'test', error.message);

    let stringError;
    try {
      stringError = assertThrows('valid function throws string error', () => {
        throw 'string error test';
      });
    } catch (e) {
      fail('assertThrows doesn\'t detect a thrown string exception');
    }
    assertEquals('string error', 'string error test', stringError);
  },

  testAssertThrowsThrowsIfJsUnitException() {
    // Asserts that assertThrows will throw a JsUnitException if the method
    // passed to assertThrows throws a JsUnitException of its own.
    // assertThrows should not be used for catching JsUnitExceptions.
    const e = assertThrowsJsUnitException(() => {
      assertThrows(() => {
        // We need to invalidate this exception so it's not flagged as a
        // legitimate failure by the test framework. The only way to get at
        // the exception thrown by assertTrue is to catch it so we can
        // invalidate it. We then need to rethrow it so the surrounding
        // assertThrows behaves as expected.
        try {
          assertTrue(false);
        } catch (ex) {
          TestCase.getActiveTestCase().invalidateAssertionException(ex);
          throw ex;
        }
      });
    });
    assertContains(
        'Function passed to assertThrows caught a JsUnitException', e.message);
  },

  testAssertThrowsJsUnitException() {
    let error = assertThrowsJsUnitException(() => {
      assertTrue(false);
    });
    assertEquals('Call to assertTrue(boolean) with false', error.message);

    error = assertThrowsJsUnitException(() => {
      assertThrowsJsUnitException(() => {
        throw new Error('fail');
      });
    });
    assertEquals(
        'Call to fail()\nExpected a JsUnitException, ' +
            'got \'Error: fail\' instead',
        error.message);

    error = assertThrowsJsUnitException(() => {
      assertThrowsJsUnitException(goog.nullFunction);
    });
    assertEquals('Expected a failure', error.message);
  },

  testAssertNotThrows() {
    if (product.SAFARI) {
      // TODO(user): Disabled so we can get the rest of the Closure
      // test suite running in a continuous build. Will investigate later.
      return;
    }

    assertThrowsJsUnitException(() => {
      assertNotThrows(
          'assertNotThrows should not pass with null param',
          /** @type {?} */ (null));
    });

    assertThrowsJsUnitException(() => {
      assertNotThrows(
          'assertNotThrows should not pass with undefined param',
          /** @type {?} */ (undefined));
    });

    assertThrowsJsUnitException(() => {
      assertNotThrows(
          'assertNotThrows should not pass with number param',
          /** @type {?} */ (1));
    });

    assertThrowsJsUnitException(() => {
      assertNotThrows(
          'assertNotThrows should not pass with string param',
          /** @type {?} */ ('string'));
    });

    assertThrowsJsUnitException(() => {
      assertNotThrows(
          'assertNotThrows should not pass with object param',
          /** @type {?} */ ({}));
    });

    let result;
    try {
      result = assertNotThrows('valid function', () => 'some value');
    } catch (e) {
      // Shouldn't be here: throw exception.
      fail('assertNotThrows returned failure on a valid function');
    }
    assertEquals(
        'assertNotThrows should return the result of the function.',
        'some value', result);

    assertThrowsJsUnitException(() => {
      assertNotThrows('non valid error throwing function', () => {
        throw new Error('a test error exception');
      });
    });
  },

  async testAssertRejects_nonThenables() {
    assertThrowsJsUnitException(() => {
      assertRejects(
          'assertRejects should not pass with null param',
          /** @type {?} */ (null));
    });

    assertThrowsJsUnitException(() => {
      assertRejects(
          'assertRejects should not pass with undefined param',
          /** @type {?} */ (undefined));
    });

    assertThrowsJsUnitException(() => {
      assertRejects(
          'assertRejects should not pass with number param',
          /** @type {?} */ (1));
    });

    assertThrowsJsUnitException(() => {
      assertRejects(
          'assertRejects should not pass with string param',
          /** @type {?} */ ('string'));
    });

    assertThrowsJsUnitException(() => {
      assertRejects(
          'assertRejects should not pass with object param with no then property',
          /** @type {?} */ ({}));
    });
  },

  testAssertRejects_deferred() {
    return internalTestAssertRejects(true, (fn) => {
      const d = new Deferred();
      try {
        fn((val) => d.callback(), (err) => d.errback(err));
      } catch (e) {
        d.errback(e);
      }
      return d;
    });
  },

  testAssertRejects_googPromise() {
    return internalTestAssertRejects(true, (fn) => new GoogPromise(fn));
  },

  testAssertRejects_promise() {
    return internalTestAssertRejects(false, (fn) => new Promise(fn));
  },

  testAssertRejects_asyncFunction_awaitingGoogPromise() {
    return internalTestAssertRejects(true, async (fn) => {
      await new GoogPromise(fn);
    });
  },

  testAssertRejects_asyncFunction_awaitingPromise() {
    return internalTestAssertRejects(false, async (fn) => {
      await new Promise(fn);
    });
  },

  testAssertRejects_asyncFunction_thatThrows() {
    return internalTestAssertRejects(false, async (fn) => {
      fn(() => {}, (err) => {
        throw err;
      });
    });
  },

  testAssertArrayEquals() {
    let a1 = [0, 1, 2];
    let a2 = [0, 1, 2];
    assertArrayEquals('Arrays should be equal', a1, a2);

    // Should have thrown because args are not arrays
    assertThrowsJsUnitException(() => {
      assertArrayEquals(true, true);
    });

    a1 = [0, undefined, 2];
    a2 = [0, , 2];
    // The following test fails unexpectedly. The bug is tracked at
    // http://code.google.com/p/closure-library/issues/detail?id=419
    // assertThrows(
    //     'assertArrayEquals distinguishes undefined items from sparse
    //     arrays', function() {
    //       assertArrayEquals(a1, a2);
    //     });

    // For the record. This behavior will probably change in the future.
    assertArrayEquals(
        'Bug: sparse arrays and undefined items are not distinguished',
        [0, undefined, 2], [0, , 2]);

    // The array elements should be compared with ===
    assertThrowsJsUnitException(() => {
      assertArrayEquals([0], ['0']);
    });

    // Arrays with different length should be different
    assertThrowsJsUnitException(() => {
      assertArrayEquals([0, undefined], [0]);
    });

    a1 = [0];
    a2 = [0];
    a2[-1] = -1;
    assertArrayEquals('Negative indexes are ignored', a1, a2);

    a1 = [0];
    a2 = [0];
    a2[/** @type {?} */ ('extra')] = 1;
    assertArrayEquals(
        'Extra properties are ignored. Use assertObjectEquals to compare them.',
        a1, a2);

    assertArrayEquals(
        'An example where assertObjectEquals would fail in IE.', ['x'],
        'x'.match(/x/g));
  },

  testAssertObjectsEqualsDifferentArrays() {
    // Should throw because args are different
    assertThrowsJsUnitException(() => {
      const a1 = ['className1'];
      const a2 = ['className2'];
      assertObjectEquals(a1, a2);
    });
  },

  testAssertObjectsEqualsNegativeArrayIndexes() {
    const a2 = [0];
    a2[-1] = -1;
    // The following test fails unexpectedly. The bug is tracked at
    // http://code.google.com/p/closure-library/issues/detail?id=418
    // assertThrows('assertObjectEquals compares negative indexes',
    // function() {
    //   assertObjectEquals(a1, a2);
    // });
  },

  testAssertObjectsEqualsDifferentTypeSameToString() {
    assertThrowsJsUnitException(() => {
      const a1 = 'className1';
      const a2 = ['className1'];
      assertObjectEquals(a1, a2);
    });

    assertThrowsJsUnitException(() => {
      const a1 = ['className1'];
      const a2 = {'0': 'className1'};
      assertObjectEquals(a1, a2);
    });

    assertThrowsJsUnitException(() => {
      const a1 = ['className1'];
      const a2 = [['className1']];
      assertObjectEquals(a1, a2);
    });
  },

  testAssertObjectsRoughlyEquals() {
    assertObjectRoughlyEquals({'a': 1}, {'a': 1.2}, 0.3);
    assertThrowsJsUnitException(
        () => {
          assertObjectRoughlyEquals({'a': 1}, {'a': 1.2}, 0.1);
        },
        'Expected <[object Object]> (Object) but was <[object Object]> ' +
            '(Object)\n   a: Expected <1> (Number) but was <1.2> (Number) which ' +
            'was more than 0.1 away');
  },

  testAssertObjectRoughlyEqualsWithStrings() {
    // Check that objects with string properties are compared properly.
    const obj1 = {'description': [{'colName': 'x1'}]};
    const obj2 = {'description': [{'colName': 'x2'}]};
    assertThrowsJsUnitException(
        () => {
          assertObjectRoughlyEquals(obj1, obj2, 0.00001);
        },
        'Expected <[object Object]> (Object)' +
            ' but was <[object Object]> (Object)' +
            '\n   description[0].colName: Expected <x1> (String) but was <x2> (String)');
    assertThrowsJsUnitException(() => {
      assertObjectRoughlyEquals('x1', 'x2', 0.00001);
    }, 'Expected <x1> (String) but was <x2> (String)');
  },

  testFindDifferences_equal() {
    assertNull(asserts.findDifferences(true, true));
    assertNull(asserts.findDifferences(null, null));
    assertNull(asserts.findDifferences(undefined, undefined));
    assertNull(asserts.findDifferences(1, 1));
    assertNull(asserts.findDifferences([1, 'a'], [1, 'a']));
    assertNull(asserts.findDifferences([[1, 2], [3, 4]], [[1, 2], [3, 4]]));
    assertNull(asserts.findDifferences([{a: 1, b: 2}], [{b: 2, a: 1}]));
    assertNull(asserts.findDifferences(null, null));
    assertNull(asserts.findDifferences(undefined, undefined));
    assertNull(asserts.findDifferences(
        new Map([['a', 1], ['b', 2]]), new Map([['b', 2], ['a', 1]])));
    assertNull(
        asserts.findDifferences(new Set(['a', 'b']), new Set(['b', 'a'])));
  },

  testFindDifferences_unequal() {
    assertNotNull(asserts.findDifferences(true, false));
    assertNotNull(asserts.findDifferences([{a: 1, b: 2}], [{a: 2, b: 1}]));
    assertNotNull(asserts.findDifferences([{a: 1}], [{a: 1, b: [2]}]));
    assertNotNull(asserts.findDifferences([{a: 1, b: [2]}], [{a: 1}]));

    assertNotNull(
        'Second map is missing key "a"; first map is missing key "b"',
        asserts.findDifferences(new Map([['a', 1]]), new Map([['b', 2]])));
    assertNotNull(
        'Value for key "a" differs by value',
        asserts.findDifferences(new Map([['a', '1']]), new Map([['a', '2']])));
    assertNotNull(
        'Value for key "a" differs by type',
        asserts.findDifferences(new Map([['a', '1']]), new Map([['a', 1]])));

    assertNotNull(
        'Second set is missing key "a"',
        asserts.findDifferences(new Set(['a', 'b']), new Set(['b'])));
    assertNotNull(
        'First set is missing key "b"',
        asserts.findDifferences(new Set(['a']), new Set(['a', 'b'])));
    assertNotNull(
        'Values have different types"',
        asserts.findDifferences(new Set(['1']), new Set([1])));
  },

  testFindDifferences_arrays_nonNaturalKeys_notConfsuedForSparseness() {
    let actual;
    let expected;

    actual = [];
    actual[1.8] = undefined;
    expected = [];
    assertNotNull(asserts.findDifferences(actual, expected));

    actual = [];
    actual[-1] = undefined;
    expected = [];
    assertNotNull(asserts.findDifferences(actual, expected));
  },

  testFindDifferences_objectsAndNull() {
    assertNotNull(asserts.findDifferences({a: 1}, null));
    assertNotNull(asserts.findDifferences(null, {a: 1}));
    assertNotNull(asserts.findDifferences(null, []));
    assertNotNull(asserts.findDifferences([], null));
    assertNotNull(asserts.findDifferences([], undefined));
  },

  testFindDifferences_basicCycle() {
    const a = {};
    const b = {};
    a.self = a;
    b.self = b;
    assertNull(asserts.findDifferences(a, b));

    a.unique = 1;
    assertNotNull(asserts.findDifferences(a, b));
  },

  testFindDifferences_crossedCycle() {
    const a = {};
    const b = {};
    a.self = b;
    b.self = a;
    assertNull(asserts.findDifferences(a, b));

    a.unique = 1;
    assertNotNull(asserts.findDifferences(a, b));
  },

  testFindDifferences_asymmetricCycle() {
    const a = {};
    const b = {};
    const c = {};
    const d = {};
    const e = {};
    a.self = b;
    b.self = a;
    c.self = d;
    d.self = e;
    e.self = c;
    assertNotNull(asserts.findDifferences(a, c));
  },

  testFindDifferences_basicCycleArray() {
    const a = [];
    const b = [];
    a[0] = a;
    b[0] = b;
    assertNull(asserts.findDifferences(a, b));

    a[1] = 1;
    assertNotNull(asserts.findDifferences(a, b));
  },

  testFindDifferences_crossedCycleArray() {
    const a = [];
    const b = [];
    a[0] = b;
    b[0] = a;
    assertNull(asserts.findDifferences(a, b));

    a[1] = 1;
    assertNotNull(asserts.findDifferences(a, b));
  },

  testFindDifferences_asymmetricCycleArray() {
    const a = [];
    const b = [];
    const c = [];
    const d = [];
    const e = [];
    a[0] = b;
    b[0] = a;
    c[0] = d;
    d[0] = e;
    e[0] = c;
    assertNotNull(asserts.findDifferences(a, c));
  },

  testFindDifferences_multiCycles() {
    const a = {};
    a.cycle1 = a;
    a.test = {cycle2: a};

    const b = {};
    b.cycle1 = b;
    b.test = {cycle2: b};
    assertNull(asserts.findDifferences(a, b));
  },

  testFindDifferences_binaryTree() {
    function createBinTree(depth, root) {
      if (depth == 0) {
        return {root: root};
      } else {
        const node = {};
        node.left = createBinTree(depth - 1, root || node);
        node.right = createBinTree(depth - 1, root || node);
        return node;
      }
    }

    // TODO(gboyer,user): This test does not terminate with the current
    // algorithm. Can be enabled when (if) the algorithm is improved.
    // assertNull(goog.testing.asserts.findDifferences(
    //    createBinTree(5, null), createBinTree(5, null)));
    assertNotNull(asserts.findDifferences(
        createBinTree(4, null), createBinTree(5, null)));
  },

  testStringSamePrefix() {
    assertThrowsJsUnitException(
        () => {
          assertEquals('abcdefghi', 'abcdefghx');
        },
        'Expected <abcdefghi> (String) but was <abcdefghx> (String)\n' +
            'Difference was at position 8. Expected [...ghi] vs. actual [...ghx]');
  },

  testStringSameSuffix() {
    assertThrowsJsUnitException(
        () => {
          assertEquals('xbcdefghi', 'abcdefghi');
        },
        'Expected <xbcdefghi> (String) but was <abcdefghi> (String)\n' +
            'Difference was at position 0. Expected [xbc...] vs. actual [abc...]');
  },

  testStringLongComparedValues() {
    assertThrowsJsUnitException(
        () => {
          assertEquals(
              'abcdefghijkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkklmnopqrstuvwxyz',
              'abcdefghijkkkkkkkkkkkkkkkkkkkkkkkkkkkkkklmnopqrstuvwxyz');
        },
        'Expected\n' +
            '<abcdefghijkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkklmnopqrstuvwxyz> (String)\n' +
            'but was\n' +
            '<abcdefghijkkkkkkkkkkkkkkkkkkkkkkkkkkkkkklmnopqrstuvwxyz> (String)\n' +
            'Difference was at position 40. Expected [...kkklmnopqrstuvwxyz] vs. actual [...kklmnopqrstuvwxyz]');
  },

  testStringLongDiff() {
    assertThrowsJsUnitException(
        () => {
          assertEquals(
              'abcdefghijkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkklmnopqrstuvwxyz',
              'abc...xyz');
        },
        'Expected\n' +
            '<abcdefghijkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkklmnopqrstuvwxyz> (String)\n' +
            'but was\n' +
            '<abc...xyz> (String)\n' +
            'Difference was at position 3. Expected\n' +
            '[...bcdefghijkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkklmnopqrstuvwxy...]\n' +
            'vs. actual\n' +
            '[...bc...xy...]');
  },

  testStringDissimilarShort() {
    assertThrowsJsUnitException(() => {
      assertEquals('x', 'y');
    }, 'Expected <x> (String) but was <y> (String)');
  },

  testStringDissimilarLong() {
    assertThrowsJsUnitException(() => {
      assertEquals('xxxxxxxxxx', 'yyyyyyyyyy');
    }, 'Expected <xxxxxxxxxx> (String) but was <yyyyyyyyyy> (String)');
  },

  testAssertElementsEquals() {
    assertElementsEquals([1, 2], [1, 2]);
    assertElementsEquals([1, 2], {0: 1, 1: 2, length: 2});
    assertElementsEquals('Good assertion', [1, 2], [1, 2]);
    assertThrowsJsUnitException(
        () => {
          assertElementsEquals('Message', [1, 2], [1]);
        },
        'length mismatch: Message\n' +
            'Expected <2> (Number) but was <1> (Number)');
  },

  testDisplayStringForValue() {
    assertEquals('<hello> (String)', _displayStringForValue('hello'));
    assertEquals('<1> (Number)', _displayStringForValue(1));
    assertEquals('<null>', _displayStringForValue(null));
    assertEquals('<undefined>', _displayStringForValue(undefined));
    assertEquals('<hello,,,,1> (Array)', _displayStringForValue([
                   'hello', /* array hole */, undefined, null, 1
                 ]));
  },

  testDisplayStringForValue_exception() {
    assertEquals(
        '<toString failed: foo message> (Object)', _displayStringForValue({
          toString: function() {
            throw new Error('foo message');
          },
        }));
  },

  testDisplayStringForValue_cycle() {
    const cycle = ['cycle'];
    cycle.push(cycle);
    assertTrue(
        'Computing string should terminate and result in a reasonable length',
        _displayStringForValue(cycle).length < 1000);
  },

  testToArrayForIterable() {
    const s = new Set([3]);
    /** @suppress {visibility} suppression added to enable type checking */
    const arr = asserts.toArray_(s);
    assertEquals(3, arr[0]);
  },
});
