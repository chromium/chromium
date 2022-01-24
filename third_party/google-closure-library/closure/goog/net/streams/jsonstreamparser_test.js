/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.streams.JsonStreamParserTest');
goog.setTestOnly();

const JsonFuzzing = goog.require('goog.labs.testing.JsonFuzzing');
const JsonStreamParser = goog.require('goog.net.streams.JsonStreamParser');
const asserts = goog.require('goog.testing.asserts');
const googJson = goog.require('goog.json');
const testSuite = goog.require('goog.testing.testSuite');
const utils = goog.require('goog.uri.utils');

let debug;

/**
 * Debug is enabled with "&debug=on" on the URL.
 * @param {string} info The debug info
 */
function print(info) {
  if (debug) {
    debug.append(
        document.createElement('p'), document.createElement('p'), info);
  }
}

// TODO(updogliu): add a fuzzy test for this.

testSuite({
  setUp() {
    const uri = window.document.URL;
    if (uri) {
      const debugFlag = utils.getParamValue(uri, 'debug');
      if (debugFlag) {
        debug = window.document.getElementById('debug');
      }
    }
  },

  testEmptyStream() {
    const parser = new JsonStreamParser();
    const result = parser.parse('[]');
    assertNull(result);
  },

  testEmptyStreamMore() {
    let parser = new JsonStreamParser();
    let result = parser.parse('  [   ]  ');
    assertNull(result);

    parser = new JsonStreamParser();
    result = parser.parse('  [   ');
    assertNull(result);

    result = parser.parse('  ]   ');
    assertNull(result);

    parser = new JsonStreamParser();
    assertThrows(() => {
      parser.parse(' a [   ');
    });
    assertThrows(() => {
      parser.parse(' [ ] ');
    });
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSingleMessage() {
    const parser = new JsonStreamParser();
    const result = parser.parse('[{"a" : "b"}]');
    assertEquals(1, result.length);
    assertEquals('b', result[0].a);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testEnclosingArray() {
    const parser = new JsonStreamParser();
    let result = parser.parse('[\n');
    assertNull(result);

    result = parser.parse('{"a" : "b"}');
    assertEquals(1, result.length);
    assertEquals('b', result[0].a);

    result = parser.parse('\n]');
    assertNull(result);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSingleMessageInChunks() {
    let parser = new JsonStreamParser();
    let result = parser.parse('[{"a" : ');
    assertNull(result);
    result = parser.parse('"b"}]');
    assertEquals(1, result.length);
    assertEquals('b', result[0].a);

    parser = new JsonStreamParser();
    result = parser.parse('[ {  "a" : ');
    assertNull(result);
    result = parser.parse('"b"} ');
    assertEquals(1, result.length);
    assertEquals('b', result[0].a);

    result = parser.parse('] ');
    assertNull(result);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testTwoMessages() {
    const parser = new JsonStreamParser();
    const result = parser.parse('[{"a" : "b"}, {"c" : "d"}]');
    assertEquals(2, result.length);
    assertEquals('b', result[0].a);
    assertEquals('d', result[1].c);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testTwoMessagesInChunks() {
    const parser = new JsonStreamParser();
    let result = parser.parse('[{"a" : "b"}, ');
    assertEquals(1, result.length);
    assertEquals('b', result[0].a);
    result = parser.parse('{"c" : "d"} ');
    assertEquals(1, result.length);
    assertEquals('d', result[0].c);
    result = parser.parse('] ');
    assertNull(result);
    assertThrows(() => {
      parser.parse('  a   ');
    });
  },

  /** Parse a fuzzy json string only once. */
  testSingleFuzzyMessages() {
    const fuzzing = new JsonFuzzing();

    // total # of tests
    for (let i = 0; i < 5; i++) {
      const data = fuzzing.newArray();
      const dataString = googJson.serialize(data);
      const parser = new JsonStreamParser();
      const result = parser.parse(dataString);

      assertEquals(data.length, result.length);
      data.forEach((elm, index) => {
        assertNotNull(elm);
        assertObjectEquals(dataString, elm, result[index]);
      });
    }
  },

  /**
   * Parse a fuzzy json string split (in two chunks) at each index of the
   * string. This is a VERY expensive test, so change the fuzzing options for
   * manual runs as required.
   */
  testChunkedFuzzyMessages() {
    const options = {jsonSize: 5, numFields: 5, arraySize: 4, maxDepth: 3};
    const fuzzing = new JsonFuzzing(options);

    const data = fuzzing.newArray();
    const dataString = googJson.serialize(data);

    print(dataString);

    for (let j = 1; j < dataString.length; j++) {
      const parser = new JsonStreamParser();
      let result = [];

      const string1 = dataString.substring(0, j);

      let parsed = parser.parse(string1);
      if (parsed) {
        result = result.concat(parsed);
      }

      const string2 = dataString.substring(j);

      parsed = parser.parse(string2);
      if (parsed) {
        result = result.concat(parsed);
      }

      assertEquals(data.length, result.length);
      data.forEach((elm, index) => {
        assertObjectEquals(dataString, elm, result[index]);
      });
    }
  },

  /** Parse a fuzzy json string in randomly generated chunks. */
  testRandomlyChunkedFuzzyMessages() {
    const fuzzing = new JsonFuzzing();

    const data = fuzzing.newArray();
    const dataString = googJson.serialize(data);

    const parser = new JsonStreamParser();

    let result = [];

    print(dataString);

    // randomly generated chunks
    let pos = 0;
    while (pos < dataString.length) {
      const num = fuzzing.nextInt(1, dataString.length - pos);
      const next = pos + num;
      const subString = dataString.substring(pos, next);

      print(subString);

      pos = next;
      const parsed = parser.parse(subString);
      if (parsed) {
        result = result.concat(parsed);
      }
    }

    assertEquals(data.length, result.length);
    data.forEach((elm, index) => {
      assertObjectEquals(
          `${dataString}
@${index}`,
          elm, result[index]);
    });
  },

  testGetExtraInput() {
    let parser = new JsonStreamParser();
    const result = parser.parse('[] , [[1, 2, 3]]');
    assertNull(result);
    assertTrue(parser.done());
    assertEquals(' , [[1, 2, 3]]', parser.getExtraInput());

    parser = new JsonStreamParser();
    assertFalse(parser.done());
    parser.parse(' [{"a" : "b"}, {"c" : "d"   ');
    assertFalse(parser.done());
    parser.parse(' } ]  a   ');
    assertTrue(parser.done());
    assertEquals('  a   ', parser.getExtraInput());
  },

  testDeliverMessageAsRawString() {
    const parser = new JsonStreamParser({'deliverMessageAsRawString': true});
    const result = parser.parse(' [{"a" : "b"}, {"c" : "d"},[],{}] ');
    assertEquals(4, result.length);
    assertEquals('{"a" : "b"}', result[0]);
    assertEquals(' {"c" : "d"}', result[1]);
    assertEquals('[]', result[2]);
    assertEquals('{}', result[3]);
  },
});
