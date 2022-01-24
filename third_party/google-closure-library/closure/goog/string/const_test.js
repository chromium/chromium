/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for Const. */

goog.module('goog.string.constTest');
goog.setTestOnly();

const Const = goog.require('goog.string.Const');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testConst() {
    const constString = Const.from('blah');
    const extracted = Const.unwrap(constString);
    assertEquals('blah', extracted);
    assertEquals('blah', constString.getTypedStringValue());
    assertEquals('Const{blah}', String(constString));

    // Interface marker is present.
    assertTrue(constString.implementsGoogStringTypedString);
  },

  /** @suppress {checkTypes} */
  testUnwrap() {
    const evil = {};
    evil.constStringValueWithSecurityContract__googStringSecurityPrivate_ =
        'evil';
    evil.CONST_STRING_TYPE_MARKER__GOOG_STRING_SECURITY_PRIVATE_ = {};

    const exception = assertThrows(() => {
      Const.unwrap(evil);
    });
    assertTrue(exception.message.indexOf('expected object of type Const') > 0);
  },

  testExplicitConstructorInvocation() {
    assertEquals('', Const.unwrap(new Const({}, 'foo')));
  },

  testBackwardsCompatibility() {
    assertEquals('', Const.unwrap(new Const()));
  },
});
