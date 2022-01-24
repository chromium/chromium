/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.json.processorTest');
goog.setTestOnly();

const NativeJsonProcessor = goog.require('goog.json.NativeJsonProcessor');
const testSuite = goog.require('goog.testing.testSuite');

const REPLACER = function(k, v) {
  return !!k ? v + 'd' : v;
};

const REVIVER = function(k, v) {
  return !!k ? v.substring(0, v.length - 1) : v;
};

// Just sanity check parsing and stringifying.
// Thorough tests are in json_test.html.

function runParsingTest(parser, input, expected) {
  assertEquals(expected, parser.stringify(parser.parse(input)));
}
testSuite({
  testNativeParser() {
    const json = '{"a":1,"b":{"c":2}}';
    runParsingTest(new NativeJsonProcessor(), json, json);
  },

  testNativeParser_withReplacer() {
    runParsingTest(
        new NativeJsonProcessor(REPLACER), '{"a":"foo","b":"goo"}',
        '{"a":"food","b":"good"}');
  },

  testNativeParser_withReviver() {
    const json = '{"a":"fod","b":"god"}';
    runParsingTest(new NativeJsonProcessor(REPLACER, REVIVER), json, json);
  },
});
