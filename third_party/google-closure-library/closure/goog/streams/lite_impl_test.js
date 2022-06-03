/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.streams.liteImplTest');
goog.setTestOnly();

const testSuite = goog.require('goog.testing.testSuite');
const {TestCases: LiteTestCases} = goog.require('goog.streams.liteTestCases');
const {newReadableStream} = goog.require('goog.streams.liteImpl');

class TestCases extends LiteTestCases {
  constructor() {
    super(newReadableStream);
  }

  testNewReadableStream_InvalidAttributes() {
    assertThrows(() => {
      newReadableStream({});
    });
    assertThrows(() => {
      newReadableStream({
        start() {},
        pull() {},
      });
    });
    assertThrows(() => {
      newReadableStream({
        start() {},
        cancel() {},
      });
    });
    assertThrows(() => {
      newReadableStream({
        start() {},
        type: 'bytes',
      });
    });
    assertThrows(() => {
      newReadableStream({
        start() {},
        autoAllocateChunkSize: 1,
      });
    });
  }
}

testSuite(new TestCases());
