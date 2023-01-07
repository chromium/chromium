/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for WireV8. */

goog.module('goog.labs.net.webChannel.WireV8Test');
goog.setTestOnly();

const WireV8 = goog.require('goog.labs.net.webChannel.WireV8');
const testSuite = goog.require('goog.testing.testSuite');

let wireCodec;

testSuite({
  setUp() {
    wireCodec = new WireV8();
  },

  tearDown() {},

  testEncodeSimpleMessage() {
    // scalar types only
    const message = {a: 'a', b: 'b'};
    const buff = [];
    wireCodec.encodeMessage(message, buff, 'prefix_');
    assertEquals(2, buff.length);
    assertEquals('prefix_a=a', buff[0]);
    assertEquals('prefix_b=b', buff[1]);
  },

  testEncodeComplexMessage() {
    const message = {a: 'a', b: {x: 1, y: 2}};
    const buff = [];
    wireCodec.encodeMessage(message, buff, 'prefix_');
    assertEquals(2, buff.length);
    assertEquals('prefix_a=a', buff[0]);
    // a round-trip URI codec
    assertEquals('prefix_b={\"x\":1,\"y\":2}', decodeURIComponent(buff[1]));
  },

  testEncodeMessageQueue() {
    const message1 = {a: 'a'};
    const queuedMessage1 = {map: message1, mapId: 3};
    const message2 = {b: 'b'};
    const queuedMessage2 = {map: message2, mapId: 4};
    const queue = [queuedMessage1, queuedMessage2];
    const result = wireCodec.encodeMessageQueue(queue, 2, null);
    assertEquals('count=2&ofs=3&req0_a=a&req1_b=b', result);
  },

  testDecodeMessage() {
    const message = wireCodec.decodeMessage('[{"a":"a", "x":1}, {"b":"b"}]');
    assertTrue(Array.isArray(message));
    assertEquals(2, message.length);
    assertEquals('a', message[0].a);
    assertEquals(1, message[0].x);
    assertEquals('b', message[1].b);
  },
});
