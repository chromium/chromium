/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.debug.ConsoleTest');
goog.setTestOnly();

const DebugConsole = goog.require('goog.debug.Console');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');
const {Level, LogRecord} = goog.require('goog.log');

let debugConsole;
let mockConsole;
let loggerName0;
let logRecord0;
let loggerName1;
let logRecord1;
let loggerName2;
let logRecord2;
let loggerName3;
let logRecord3;

/**
 * Logs the message at all log levels.
 * @param {string} message The message to log.
 */
function logAtAllLevels(message) {
  logAtLevel(Level.SHOUT, message);
  logAtLevel(Level.SEVERE, message);
  logAtLevel(Level.WARNING, message);
  logAtLevel(Level.INFO, message);
  logAtLevel(Level.CONFIG, message);
  logAtLevel(Level.FINE, message);
  logAtLevel(Level.FINER, message);
  logAtLevel(Level.FINEST, message);
  logAtLevel(Level.ALL, message);
}

/**
 * Adds a log record to the debug console.
 * @param {!Level} level The level at which to log.
 * @param {string} message The message to log.
 */
function logAtLevel(level, message) {
  debugConsole.addLogRecord(new LogRecord(level, message, loggerName1));
}
testSuite({
  setUp() {
    debugConsole = new DebugConsole();

    // Set up a recorder for mockConsole.log
    mockConsole = {log: recordFunction()};
    /** @suppress {visibility} suppression added to enable type checking */
    DebugConsole.console_ = mockConsole;

    loggerName0 = 'debug.logger';
    logRecord0 =
        new LogRecord(Level.FINE, 'blah blah blah no one cares', loggerName0);

    // Test logger 1.
    loggerName1 = 'this.is.a.logger';
    logRecord1 = new LogRecord(Level.INFO, 'this is a statement', loggerName1);

    // Test logger 2.
    loggerName2 = 'name.of.logger';
    logRecord2 =
        new LogRecord(Level.WARNING, 'hey, this is a warning', loggerName2);

    // Test logger 3.
    loggerName3 = 'third.logger';
    logRecord3 = new LogRecord(
        Level.SEVERE, 'seriously, this statement is serious', loggerName3);
  },

  testLoggingWithSimpleConsole() {
    // Make sure all messages use the log function.
    logAtAllLevels('test message');
    assertEquals(9, mockConsole.log.getCallCount());
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testLoggingWithDebugSupported() {
    // Make sure the log function is the default when only 'debug' is available.
    mockConsole['debug'] = recordFunction();
    logAtAllLevels('test message');
    assertEquals(5, mockConsole.log.getCallCount());
    assertEquals(4, mockConsole.debug.getCallCount());
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testLoggingWithErrorSupported() {
    // Make sure the log function is the default when only 'error' is available.
    mockConsole['error'] = recordFunction();
    logAtAllLevels('test message');
    assertEquals(2, mockConsole.error.getCallCount());
    assertEquals(7, mockConsole.log.getCallCount());
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testLoggingWithWarningSupported() {
    // Make sure the log function is the default when only 'warn' is available.
    mockConsole['warn'] = recordFunction();
    logAtAllLevels('test message');
    assertEquals(1, mockConsole.warn.getCallCount());
    assertEquals(8, mockConsole.log.getCallCount());
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testLoggingWithEverythingSupported() {
    mockConsole['debug'] = recordFunction();
    mockConsole['error'] = recordFunction();
    mockConsole['warn'] = recordFunction();
    mockConsole['log'] = recordFunction();
    logAtAllLevels('test message');
    assertEquals(4, mockConsole.debug.getCallCount());
    assertEquals(2, mockConsole.error.getCallCount());
    assertEquals(1, mockConsole.warn.getCallCount());
    assertEquals(2, mockConsole.log.getCallCount());
  },

  testAddLogRecordWithoutFilters() {
    // Make sure none are filtered.
    debugConsole.addLogRecord(logRecord1);
    assertEquals(1, mockConsole.log.getCallCount());
    debugConsole.addLogRecord(logRecord2);
    assertEquals(2, mockConsole.log.getCallCount());
    debugConsole.addLogRecord(logRecord3);
    assertEquals(3, mockConsole.log.getCallCount());
  },

  testAddLogRecordWithOneFilter() {
    // Filter #2 and make sure the filtering is correct for all records.
    debugConsole.addFilter(loggerName2);
    debugConsole.addLogRecord(logRecord1);
    assertEquals(1, mockConsole.log.getCallCount());
    debugConsole.addLogRecord(logRecord2);
    assertEquals(1, mockConsole.log.getCallCount());
    debugConsole.addLogRecord(logRecord3);
    assertEquals(2, mockConsole.log.getCallCount());
  },

  testAddLogRecordWithMoreThanOneFilter() {
    // Filter #1 and #3 and check.
    debugConsole.addFilter(loggerName1);
    debugConsole.addFilter(loggerName3);
    debugConsole.addLogRecord(logRecord1);
    assertEquals(0, mockConsole.log.getCallCount());
    debugConsole.addLogRecord(logRecord2);
    assertEquals(1, mockConsole.log.getCallCount());
    debugConsole.addLogRecord(logRecord3);
    assertEquals(1, mockConsole.log.getCallCount());
  },

  testAddLogRecordWithAddAndRemoveFilter() {
    debugConsole.addFilter(loggerName1);
    debugConsole.addFilter(loggerName2);
    debugConsole.removeFilter(loggerName1);
    debugConsole.removeFilter(loggerName2);
    debugConsole.addLogRecord(logRecord1);
    assertEquals(1, mockConsole.log.getCallCount());
    debugConsole.addLogRecord(logRecord2);
    assertEquals(2, mockConsole.log.getCallCount());
    debugConsole.addLogRecord(logRecord3);
    assertEquals(3, mockConsole.log.getCallCount());
  },

  testSetConsole() {
    const fakeConsole = {log: recordFunction()};

    logAtLevel(Level.INFO, 'test message 1');
    logAtAllLevels('test message 1');
    assertEquals(0, fakeConsole.log.getCallCount());

    DebugConsole.setConsole(fakeConsole);

    logAtLevel(Level.INFO, 'test message 2');
    assertEquals(1, fakeConsole.log.getCallCount());
  },
});
