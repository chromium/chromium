/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.string.StringBufferTest');
goog.setTestOnly();

const StringBuffer = goog.require('goog.string.StringBuffer');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testStringBuffer() {
    let sb = new StringBuffer();
    sb.append('Four score');
    sb.append(' ');
    sb.append('and seven years ago.');
    assertEquals('Test 1', 'Four score and seven years ago.', sb.toString());

    sb.clear();
    assertEquals('Test 2', '', sb.toString());

    sb = new StringBuffer('Four score ');
    sb.append('and seven years ago.');
    assertEquals('Test 3', 'Four score and seven years ago.', sb.toString());

    // can pass in non-Strings?
    sb = new StringBuffer(1);
    sb.append(2);
    assertEquals('Test 4', '12', sb.toString());
  },

  testStringBufferSet() {
    const sb = new StringBuffer('foo');
    sb.set('bar');
    assertEquals('Test 1', 'bar', sb.toString());
  },

  testStringBufferMultiAppend() {
    let sb = new StringBuffer('hey', 'foo');
    sb.append('bar', 'baz');
    assertEquals('Test 1', 'heyfoobarbaz', sb.toString());

    sb = new StringBuffer();
    sb.append(1, 2);
    // should not add up to '3'
    assertEquals('Test 2', '12', sb.toString());
  },

  testStringBufferToString() {
    const sb = new StringBuffer('foo', 'bar');
    assertEquals('Test 1', 'foobar', sb.toString());
  },

  testStringBufferWithFalseFirstArgument() {
    let sb = new StringBuffer(0, 'more');
    assertEquals('Test 1', '0more', sb.toString());

    sb = new StringBuffer(false, 'more');
    assertEquals('Test 2', 'falsemore', sb.toString());

    sb = new StringBuffer('', 'more');
    assertEquals('Test 3', 'more', sb.toString());

    sb = new StringBuffer(null, 'more');
    assertEquals('Test 4', '', sb.toString());

    sb = new StringBuffer(undefined, 'more');
    assertEquals('Test 5', '', sb.toString());
  },

  testStringBufferGetLength() {
    const sb = new StringBuffer();
    assertEquals(0, sb.getLength());

    sb.append('foo');
    assertEquals(3, sb.getLength());

    sb.append('baroo');
    assertEquals(8, sb.getLength());

    sb.clear();
    assertEquals(0, sb.getLength());
  },
});
