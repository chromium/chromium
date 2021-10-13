/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.messaging.LoggerClientTest');
goog.setTestOnly();

const LoggerClient = goog.require('goog.messaging.LoggerClient');
const MockControl = goog.require('goog.testing.MockControl');
const MockMessageChannel = goog.require('goog.testing.messaging.MockMessageChannel');
const debug = goog.require('goog.debug');
const testSuite = goog.require('goog.testing.testSuite');
const {Level, getLogger, warning} = goog.require('goog.log');

let mockControl;
let channel;
let client;
let logger;

testSuite({
  setUp() {
    /** Used computed properties to avoid compiler checks of the define */
    debug['FORCE_SLOPPY_STACKS'] = false;
    mockControl = new MockControl();
    channel = new MockMessageChannel(mockControl);
    client = new LoggerClient(channel, 'log');
    logger = getLogger('test.logging.Object');
  },

  tearDown() {
    channel.dispose();
    client.dispose();
  },

  testCommand() {
    channel.send('log', {
      name: 'test.logging.Object',
      level: Level.WARNING.value,
      message: 'foo bar',
      exception: undefined,
    });
    mockControl.$replayAll();
    warning(logger, 'foo bar');
    mockControl.$verifyAll();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testCommandWithException() {
    const ex = Error('oh no');
    /** @suppress {checkTypes} suppression added to enable type checking */
    ex.stack = ['one', 'two'];
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    ex.message0 = 'message 0';
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    ex.message1 = 'message 1';
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    ex.ignoredProperty = 'ignored';

    channel.send('log', {
      name: 'test.logging.Object',
      level: Level.WARNING.value,
      message: 'foo bar',
      exception: {
        name: 'Error',
        message: ex.message,
        stack: ex.stack,
        lineNumber: ex.lineNumber || ex.line || 'Not available',
        fileName: ex.fileName || ex.sourceURL || window.location.href,
        message0: ex.message0,
        message1: ex.message1,
      },
    });
    mockControl.$replayAll();
    warning(logger, 'foo bar', ex);
    mockControl.$verifyAll();
  },
});
