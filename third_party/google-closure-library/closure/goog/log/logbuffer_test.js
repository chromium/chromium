/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.log.LogBufferTest');
goog.setTestOnly();

const Level = goog.require('goog.log.Level');
const LogBuffer = goog.require('goog.log.LogBuffer');
const testSuite = goog.require('goog.testing.testSuite');

const PLACEHOLDER_LEVELS = [
  Level.INFO,
  Level.WARNING,
  Level.SEVERE,
];
const PLACEHOLDER_MESSAGES = ['a', 'b', 'c'];
const PLACEHOLDER_NAMES = ['X', 'Y', 'Z'];
const CAPACITY = 4;

let buffer;
let placeholderIndex = 0;

function verifyRecord(expectedIndex, record) {
  const index = expectedIndex % PLACEHOLDER_MESSAGES.length;
  const message = PLACEHOLDER_MESSAGES[index];
  const level = PLACEHOLDER_LEVELS[index];
  const name = PLACEHOLDER_NAMES[index];
  assertEquals(
      `Wrong level for record ${expectedIndex}`, level, record.getLevel());
  assertEquals(
      `Wrong message for record ${expectedIndex}`, message,
      record.getMessage());
  assertEquals(
      `Wrong name for record ${expectedIndex}`, name, record.getLoggerName());
}

function addAndVerifyRecord() {
  const index = placeholderIndex % PLACEHOLDER_MESSAGES.length;
  const level = PLACEHOLDER_LEVELS[index];
  const message = PLACEHOLDER_MESSAGES[index];
  const name = PLACEHOLDER_NAMES[index];
  const record = buffer.addRecord(level, message, name);
  verifyRecord(placeholderIndex, record);
  placeholderIndex++;
}

function addSomeRecords(howMany) {
  for (let i = 0; i < howMany; i++) {
    addAndVerifyRecord();
  }
}

testSuite({
  setUp() {
    buffer = new LogBuffer(CAPACITY);
  },

  testAddRecord() {
    addSomeRecords(CAPACITY * 3);
  },

  testIsFull() {
    assertFalse('Should not be full.', buffer.isFull());
    addSomeRecords(CAPACITY * 1.5);
    assertTrue('Should be full.', buffer.isFull());
    buffer.clear();
    assertFalse('Should not be full after clear().', buffer.isFull());
    addSomeRecords(CAPACITY - 1);
    assertFalse('Should not be full but almost full.', buffer.isFull());
  },

  testForEachRecord() {
    // Test with it half full.
    const howMany1 = CAPACITY / 2;
    addSomeRecords(howMany1);
    let counter1 = 0;
    buffer.forEachRecord((record) => {
      verifyRecord(counter1++, record);
    });
    assertEquals('Wrong number of records when half full.', howMany1, counter1);

    // Test with it full.
    const howMany2 = CAPACITY;
    addSomeRecords(howMany2);
    let index = counter1;
    buffer.forEachRecord((record) => {
      verifyRecord(index++, record);
    });
    assertEquals(
        'Wrong number of records when full.', howMany1 + howMany2, index);
  },
});
