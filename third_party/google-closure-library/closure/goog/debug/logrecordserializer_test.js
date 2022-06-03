// Copyright 2011 The Closure Library Authors. All Rights Reserved.
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

goog.module('goog.debug.logRecordSerializerTest');
goog.setTestOnly();

const LogRecord = goog.require('goog.debug.LogRecord');
const Logger = goog.require('goog.debug.Logger');
const logRecordSerializer = goog.require('goog.debug.logRecordSerializer');
const testSuite = goog.require('goog.testing.testSuite');

const NOW = 1311484654000;
const SEQ = 1231;

testSuite({
  testBasic() {
    const rec = new LogRecord(
        Logger.Level.FINE, 'An awesome message', 'logger.name', NOW, SEQ);
    const thawed =
        logRecordSerializer.parse(logRecordSerializer.serialize(rec));

    assertEquals(Logger.Level.FINE, thawed.getLevel());
    assertEquals('An awesome message', thawed.getMessage());
    assertEquals('logger.name', thawed.getLoggerName());
    assertEquals(NOW, thawed.getMillis());
    assertEquals(SEQ, thawed.getSequenceNumber());
    assertNull(thawed.getException());
  },

  testWithException() {
    const err = new Error('it broke!');
    const rec = new LogRecord(
        Logger.Level.FINE, 'An awesome message', 'logger.name', NOW, SEQ);
    rec.setException(err);
    const thawed =
        logRecordSerializer.parse(logRecordSerializer.serialize(rec));
    assertEquals(err.message, thawed.getException().message);
  },

  testCustomLogLevel() {
    const rec = new LogRecord(
        new Logger.Level('CUSTOM', -1), 'An awesome message', 'logger.name',
        NOW, SEQ);
    const thawed =
        logRecordSerializer.parse(logRecordSerializer.serialize(rec));

    assertEquals('CUSTOM', thawed.getLevel().name);
    assertEquals(-1, thawed.getLevel().value);
  },

  testWeirdLogLevel() {
    const rec = new LogRecord(
        new Logger.Level('FINE', -1), 'An awesome message', 'logger.name', NOW,
        SEQ);
    const thawed =
        logRecordSerializer.parse(logRecordSerializer.serialize(rec));

    assertEquals('FINE', thawed.getLevel().name);
    // Makes sure that the log leve is still -1 even though the name
    // FINE is predefind.
    assertEquals(-1, thawed.getLevel().value);
  },
});
