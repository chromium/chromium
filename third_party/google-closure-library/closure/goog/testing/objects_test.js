/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Tests for goog.testing.objects.
 */
goog.module('goog.testing.objects_test');
goog.setTestOnly('goog.testing.objects_test');

const asserts = goog.require('goog.testing.asserts');
const testSuite = goog.require('goog.testing.testSuite');
const {assertDoesNotRetainReference, assertRetainsReference} = goog.require('goog.testing.objects');

testSuite({
  testSimpleSearches() {
    const needle = {};
    const hayStack = {a: {a: {a: needle}}};
    assertRetainsReference(hayStack, needle);
    assertDoesNotRetainReference(hayStack, {});

    asserts.assertThrowsJsUnitException(
        () => {
          assertDoesNotRetainReference(hayStack, needle);
        },
        `expected there to be no retention path, found the value @ ` +
            `object['a']['a']['a']`);
    asserts.assertThrowsJsUnitException(() => {
      assertRetainsReference(hayStack, {});
    }, `The object does not transitively retain a reference to the given value`);
  },
  // make sure we handle recursive structures
  testComplexStructure_recursive() {
    const needle = {};
    class A {
      constructor() {
        this.needle = needle;
      }
    }
    class B extends A {
      constructor() {
        super();
        this.selfEdge = {someBox: this};
      }
    }
    assertRetainsReference(new B(), needle);
    assertDoesNotRetainReference(new B(), 'notFound');
    asserts.assertThrowsJsUnitException(
        () => {
          assertDoesNotRetainReference(new B(), needle);
        },
        `expected there to be no retention path, found the value @ ` +
            `object['needle']`);
    asserts.assertThrowsJsUnitException(() => {
      assertRetainsReference(new B(), 'notFound');
    }, `The object does not transitively retain a reference to the given value`);
  },
  testComplexStructure_recursive_deep() {
    const needle = {};
    const end = {a: needle};
    let head = end;
    for (let i = 0; i < 1000; i++) {
      head = {a: head};
    }
    end.b = head;
    assertRetainsReference(head, needle);
    assertDoesNotRetainReference(head, 'notFound');
    asserts.assertThrowsJsUnitException(() => {
      assertDoesNotRetainReference(head, needle);
    });
    asserts.assertThrowsJsUnitException(() => {
      assertRetainsReference(head, 'notFound');
    });
  }
});
