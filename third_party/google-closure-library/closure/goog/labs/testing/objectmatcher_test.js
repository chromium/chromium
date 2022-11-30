/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.testing.objectMatcherTest');
goog.setTestOnly();

const MatcherError = goog.require('goog.labs.testing.MatcherError');
const assertThat = goog.require('goog.labs.testing.assertThat');
/** @suppress {extraRequire} */
const matchers = goog.require('goog.labs.testing');
const testSuite = goog.require('goog.testing.testSuite');

function assertMatcherError(callable, errorString) {
  const e = assertThrows(errorString || 'callable throws exception', callable);
  assertTrue(e instanceof MatcherError);
}
testSuite({
  testAnyObject() {
    assertThat({}, anyObject(), 'typeof {} == "object"');
    assertMatcherError(() => {
      assertThat(null, anyObject());
    }, 'typeof null == "object"');
  },

  testObjectEquals() {
    const obj1 = {x: 1};
    const obj2 = obj1;
    assertThat(obj1, equalsObject(obj2), 'obj1 equals obj2');

    assertMatcherError(() => {
      assertThat({x: 1}, equalsObject({}));
    }, 'equalsObject does not throw exception on failure');
  },

  testInstanceOf() {
    function expected() {
      this.x = 1;
    }
    /** @suppress {checkTypes} suppression added to enable type checking */
    const input = new expected();
    assertThat(
        input, instanceOfClass(expected), 'input is an instance of expected');

    assertMatcherError(() => {
      assertThat(5, instanceOfClass(() => {}));
    }, 'instanceOfClass does not throw exception on failure');
  },

  testHasProperty() {
    assertThat({x: 1}, hasProperty('x'), '{x:1} has property x}');

    assertMatcherError(() => {
      assertThat({x: 1}, hasProperty('y'));
    }, 'hasProperty does not throw exception on failure');
  },

  testIsNull() {
    assertThat(null, isNull(), 'null is null');

    assertMatcherError(() => {
      assertThat(5, isNull());
    }, 'isNull does not throw exception on failure');
  },

  testIsNullOrUndefined() {
    let x;
    assertThat(
        undefined, isNullOrUndefined(), 'undefined is null or undefined');
    assertThat(x, isNullOrUndefined(), 'undefined is null or undefined');
    x = null;
    assertThat(null, isNullOrUndefined(), 'null is null or undefined');
    assertThat(x, isNullOrUndefined(), 'null is null or undefined');

    assertMatcherError(() => {
      assertThat(5, isNullOrUndefined());
    }, 'isNullOrUndefined does not throw exception on failure');
  },

  testIsUndefined() {
    let x;
    assertThat(undefined, isUndefined(), 'undefined is undefined');
    assertThat(x, isUndefined(), 'undefined is undefined');

    assertMatcherError(() => {
      assertThat(5, isUndefined());
    }, 'isUndefined does not throw exception on failure');
  },
});
