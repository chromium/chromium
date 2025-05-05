/**
 * @fileoverview Tests for whitespace module functions.
 */
goog.module('goog.testing.i18n.whitespace_test');
goog.setTestOnly();

const testSuite = goog.require('goog.testing.testSuite');
const {assertEquals} = goog.require('goog.testing.asserts');
const {removeWhitespace} = goog.require('goog.testing.i18n.whitespace');

testSuite({
  testWhitespaceNormalization() {
    assertEquals('', removeWhitespace('\u1680'));
    assertEquals('ab', removeWhitespace('a\u3000b'));
    assertEquals('abc', removeWhitespace('\ta\u00a0\u0020b\u205fc'));
    assertEquals('xy', removeWhitespace('x\u0020y'));
    assertEquals('xy', removeWhitespace('x\u202fy'));
    assertEquals('xy', removeWhitespace('x\t\u00a0y'));
  },
});
