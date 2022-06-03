/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Unit tests for log.
 */

goog.module('goog.logTest');
goog.setTestOnly();

const Level = goog.require('goog.log.Level');
const log = goog.require('goog.log');
const testSuite = goog.require('goog.testing.testSuite');

/**
 * A simple log handler that remembers the last record published.
 */
class TestHandler {
  constructor() {
    this.logRecord = null;
    this.onPublish = (logRecord) => {
      this.logRecord = logRecord;
    };
  }

  reset() {
    this.logRecord = null;
  }
}

testSuite({
  testLogging() {
    // Test that when a message is logged with a given logger, a handler for the
    // logger is called with that message.
    const handler = new TestHandler();
    const logger = log.getLogger('goog.logTest.bar.baaz');
    try {
      log.addHandler(logger, handler.onPublish);
      log.log(logger, Level.WARNING, 'foo');
      assertNotNull(handler.logRecord);
      assertEquals(Level.WARNING, handler.logRecord.getLevel());
      assertEquals('foo', handler.logRecord.getMessage());
      handler.reset();
    } finally {
      log.removeHandler(logger, handler.onPublish);
    }
    log.log(logger, Level.WARNING, 'foo');
    assertNull(handler.logRecord);
  },

  testParentLogHandling() {
    // Test that when a message is logged with a given logger, a handler for the
    // logger's ancestry chain is called with that message.
    const handler = new TestHandler();
    const logger = log.getLogger('goog.logTest.bar.baaz');
    const parentLogger = log.getLogger('goog.logTest.bar');
    try {
      log.addHandler(parentLogger, handler.onPublish);
      log.log(logger, Level.WARNING, 'ancestral foo');
      assertNotNull(handler.logRecord);
      assertEquals(Level.WARNING, handler.logRecord.getLevel());
      assertEquals('ancestral foo', handler.logRecord.getMessage());
      handler.reset();
    } finally {
      log.removeHandler(parentLogger, handler.onPublish);
    }
    log.log(logger, Level.WARNING, 'foo');
    assertNull(handler.logRecord);
  },

  testRootLogHandler() {
    // Test that when a message is logged with any logger, a handler for the
    // root logger is called with that message.
    const root = log.getRootLogger();
    const handler = new TestHandler();
    const logger = log.getLogger('goog.logTest.bar.baaz');
    try {
      log.addHandler(root, handler.onPublish);
      log.log(logger, Level.WARNING, 'prehistoric foo');
      assertNotNull(handler.logRecord);
      assertEquals(Level.WARNING, handler.logRecord.getLevel());
      assertEquals('prehistoric foo', handler.logRecord.getMessage());
      handler.reset();
    } finally {
      log.removeHandler(root, handler.onPublish);
    }
    log.log(logger, Level.WARNING, 'foo');
    assertNull(handler.logRecord);
  },

  testLogFilteringByLevel() {
    // Test that when a message is logged with any logger at a given level,
    // a log handler for that logger is only called with that message if the
    // logger's own level not higher.
    const handler = new TestHandler();
    const logger = log.getLogger('goog.logTest.bar.baaz.warn', Level.WARNING);
    try {
      log.addHandler(logger, handler.onPublish);
      log.log(logger, Level.SEVERE, 'foo');
      assertNotNull(handler.logRecord);
      assertEquals(Level.SEVERE, handler.logRecord.getLevel());
      assertEquals('foo', handler.logRecord.getMessage());
      handler.reset();

      log.log(logger, Level.INFO, 'unimportant foo');
      assertNull(handler.logRecord);
    } finally {
      log.removeHandler(logger, handler.onPublish);
    }
  },

  testLoggingCallback() {
    // Test that when a callback (representing a lazily-initialized message) is
    // logged with a logger, the return value of the callback is treated as the
    // message.
    // In addition, test that the callback is not invoked if the message doesn't
    // need to be logged (here, because its level is too low).
    const handler = new TestHandler();
    const logger = log.getLogger('goog.logTest.bar.baaz.warn', Level.WARNING);
    try {
      log.addHandler(logger, handler.onPublish);
      log.log(logger, Level.SEVERE, () => 'foo');
      assertNotNull(handler.logRecord);
      assertEquals(Level.SEVERE, handler.logRecord.getLevel());
      assertEquals('foo', handler.logRecord.getMessage());
      handler.reset();

      let callbackTriggered = false;
      log.log(logger, Level.INFO, () => {
        callbackTriggered = true;
        return 'side-effectful foo';
      });
      assert(!callbackTriggered);
      assertNull(handler.logRecord);
    } finally {
      log.removeHandler(logger, handler.onPublish);
    }
  },

  testLoggingWithException() {
    // Test that when a message and an exception are logged with a given logger,
    // a handler for the logger is called with both.
    const handler = new TestHandler();
    const logger = log.getLogger('goog.logTest.bar.baaz');
    const exception = new Error();
    try {
      log.addHandler(logger, handler.onPublish);
      log.log(logger, Level.WARNING, 'exceptional foo', exception);
      assertNotNull(handler.logRecord);
      assertEquals(Level.WARNING, handler.logRecord.getLevel());
      assertEquals('exceptional foo', handler.logRecord.getMessage());
      assertEquals(exception, handler.logRecord.getException());
    } finally {
      log.removeHandler(logger, handler.onPublish);
    }
  },

  testPublishingLogRecord() {
    // Test getting and publishing a log record.
    const handler = new TestHandler();
    const logger = log.getLogger('goog.logTest.bar.baaz');
    const exception = new Error();
    try {
      log.addHandler(logger, handler.onPublish);
      const logRecord =
          log.getLogRecord(logger, Level.WARNING, 'foo', exception);
      assertEquals(logRecord.getLoggerName(), 'goog.logTest.bar.baaz');
      assertEquals(logRecord.getLevel(), Level.WARNING);
      assertEquals(logRecord.getMessage(), 'foo');
      assertEquals(logRecord.getException(), exception);
      // Should not have published anything.
      assertNull(handler.logRecord);

      log.publishLogRecord(logger, logRecord);
      assertEquals(handler.logRecord, logRecord);
      handler.reset();
    } finally {
      log.removeHandler(logger, handler.onPublish);
    }
  }
});
