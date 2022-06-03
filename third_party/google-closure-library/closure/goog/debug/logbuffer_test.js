// Copyright 2010 The Closure Library Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

goog.module('goog.debug.LogBufferTest');
goog.setTestOnly();

const LogBuffer = goog.require('goog.debug.LogBuffer');
const Logger = goog.require('goog.debug.Logger');
const testSuite = goog.require('goog.testing.testSuite');

const DUMMY_LEVELS = [
  Logger.Level.INFO,
  Logger.Level.WARNING,
  Logger.Level.SEVERE,
];
const DUMMY_MESSAGES = ['a', 'b', 'c'];
const DUMMY_NAMES = ['X', 'Y', 'Z'];

let buffer;
let dummyIndex = 0;

function verifyRecord(expectedIndex, record) {
  const index = expectedIndex % DUMMY_MESSAGES.length;
  const message = DUMMY_MESSAGES[index];
  const level = DUMMY_LEVELS[index];
  const name = DUMMY_NAMES[index];
  assertEquals(
      `Wrong level for record ${expectedIndex}`, level, record.getLevel());
  assertEquals(
      `Wrong message for record ${expectedIndex}`, message,
      record.getMessage());
  assertEquals(
      `Wrong name for record ${expectedIndex}`, name, record.getLoggerName());
}

function addAndVerifyRecord() {
  const index = dummyIndex % DUMMY_MESSAGES.length;
  const level = DUMMY_LEVELS[index];
  const message = DUMMY_MESSAGES[index];
  const name = DUMMY_NAMES[index];
  const record = buffer.addRecord(level, message, name);
  verifyRecord(dummyIndex, record);
  dummyIndex++;
}

function addSomeRecords(howMany) {
  for (let i = 0; i < howMany; i++) {
    addAndVerifyRecord();
  }
}

testSuite({
  setUp() {
    LogBuffer.CAPACITY = 4;
    LogBuffer.instance_ = null;
    buffer = LogBuffer.getInstance();
  },

  testAddRecord() {
    addSomeRecords(LogBuffer.CAPACITY * 3);
  },

  testIsFull() {
    assertFalse('Should not be full.', buffer.isFull_);
    addSomeRecords(LogBuffer.CAPACITY * 1.5);
    assertTrue('Should be full.', buffer.isFull_);
    buffer.clear();
    assertFalse('Should not be full after clear().', buffer.isFull_);
    addSomeRecords(LogBuffer.CAPACITY - 1);
    assertFalse('Should not be full but almost full.', buffer.isFull_);
  },

  testForEachRecord() {
    // Test with it half full.
    const howMany1 = LogBuffer.CAPACITY / 2;
    addSomeRecords(howMany1);
    let counter1 = 0;
    buffer.forEachRecord((record) => {
      verifyRecord(counter1++, record);
    });
    assertEquals('Wrong number of records when half full.', howMany1, counter1);

    // Test with it full.
    const howMany2 = LogBuffer.CAPACITY;
    addSomeRecords(howMany2);
    let index = counter1;
    buffer.forEachRecord((record) => {
      verifyRecord(index++, record);
    });
    assertEquals(
        'Wrong number of records when full.', howMany1 + howMany2, index);
  },
});
