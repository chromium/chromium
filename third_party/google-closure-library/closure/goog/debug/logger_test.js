// Copyright 2006 The Closure Library Authors. All Rights Reserved.
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

goog.module('goog.debug.LoggerTest');
goog.setTestOnly();

const LogManager = goog.require('goog.debug.LogManager');
const Logger = goog.require('goog.debug.Logger');
const testSuite = goog.require('goog.testing.testSuite');

function getDebug(sb, logger, level) {
  let spacer = '';
  if (level) {
    spacer = (new Array(level + 1)).join(' ');
  }
  sb[sb.length] = spacer;
  const name = logger.getName();
  if (name) {
    sb[sb.length] = name;
  } else {
    sb[sb.length] = 'ROOT';
  }
  sb[sb.length] = '\n';
  const children = logger.getChildren();
  for (let key in children) {
    getDebug(sb, children[key], level + 1);
  }
}

class TestHandler {
  constructor() {
    this.logRecord = null;
  }

  onPublish(logRecord) {
    this.logRecord = logRecord;
  }

  reset() {
    this.logRecord = null;
  }
}

testSuite({
  testParents() {
    const l1 = Logger.getLogger('goog.test');
    const l2 = Logger.getLogger('goog.bar');
    const l3 = Logger.getLogger('goog.bar.foo');
    const l4 = Logger.getLogger('goog.bar.baaz');
    const rootLogger = LogManager.getRoot();
    const googLogger = Logger.getLogger('goog');
    assertEquals(rootLogger, googLogger.getParent());
    assertEquals(googLogger, l1.getParent());
    assertEquals(googLogger, l2.getParent());
    assertEquals(l2, l3.getParent());
    assertEquals(l2, l4.getParent());
  },

  testLogging() {
    const root = LogManager.getRoot();
    const handler = new TestHandler();
    const f = goog.bind(handler.onPublish, handler);
    root.addHandler(f);
    const l4 = Logger.getLogger('goog.bar.baaz');
    l4.log(Logger.Level.WARNING, 'foo');
    assertNotNull(handler.logRecord);
    assertEquals(Logger.Level.WARNING, handler.logRecord.getLevel());
    assertEquals('foo', handler.logRecord.getMessage());
    handler.logRecord = null;
    root.removeHandler(f);
    l4.log(Logger.Level.WARNING, 'foo');
    assertNull(handler.logRecord);
  },

  testFiltering() {
    const root = LogManager.getRoot();
    const handler = new TestHandler();
    const f = goog.bind(handler.onPublish, handler);
    root.addHandler(f);
    const l3 = Logger.getLogger('goog.bar.foo');
    l3.setLevel(Logger.Level.WARNING);
    const l4 = Logger.getLogger('goog.bar.baaz');
    l4.setLevel(Logger.Level.INFO);
    l4.log(Logger.Level.WARNING, 'foo');
    assertNotNull(handler.logRecord);
    assertEquals(Logger.Level.WARNING, handler.logRecord.getLevel());
    assertEquals('foo', handler.logRecord.getMessage());
    handler.reset();
    l3.log(Logger.Level.INFO, 'bar');
    assertNull(handler.logRecord);
    l3.log(Logger.Level.WARNING, 'baaz');
    assertNotNull(handler.logRecord);
    handler.reset();
    l3.log(Logger.Level.SEVERE, 'baaz');
    assertNotNull(handler.logRecord);
  },

  testException() {
    const root = LogManager.getRoot();
    const handler = new TestHandler();
    const f = goog.bind(handler.onPublish, handler);
    root.addHandler(f);
    const logger = Logger.getLogger('goog.debug.logger_test');
    const ex = Error('boo!');
    logger.severe('hello', ex);
    assertNotNull(handler.logRecord);
    assertEquals(Logger.Level.SEVERE, handler.logRecord.getLevel());
    assertEquals('hello', handler.logRecord.getMessage());
    assertEquals(ex, handler.logRecord.getException());
    assertEquals('boo!', handler.logRecord.getException().message);
  },

  testMessageCallbacks() {
    const root = LogManager.getRoot();
    const handler = new TestHandler();
    const f = goog.bind(handler.onPublish, handler);
    root.addHandler(f);
    const l3 = Logger.getLogger('goog.bar.foo');
    l3.setLevel(Logger.Level.WARNING);

    l3.log(Logger.Level.INFO, () => {
      throw 'Message callback shouldn\'t be called when below logger\'s level!';
    });
    assertNull(handler.logRecord);

    l3.log(Logger.Level.WARNING, () => 'heya');
    assertNotNull(handler.logRecord);
    assertEquals(Logger.Level.WARNING, handler.logRecord.getLevel());
    assertEquals('heya', handler.logRecord.getMessage());
  },

  testGetPredefinedLevel() {
    assertEquals(Logger.Level.OFF, Logger.Level.getPredefinedLevel('OFF'));
    assertEquals(Logger.Level.SHOUT, Logger.Level.getPredefinedLevel('SHOUT'));
    assertEquals(
        Logger.Level.SEVERE, Logger.Level.getPredefinedLevel('SEVERE'));
    assertEquals(
        Logger.Level.WARNING, Logger.Level.getPredefinedLevel('WARNING'));
    assertEquals(Logger.Level.INFO, Logger.Level.getPredefinedLevel('INFO'));
    assertEquals(
        Logger.Level.CONFIG, Logger.Level.getPredefinedLevel('CONFIG'));
    assertEquals(Logger.Level.FINE, Logger.Level.getPredefinedLevel('FINE'));
    assertEquals(Logger.Level.FINER, Logger.Level.getPredefinedLevel('FINER'));
    assertEquals(
        Logger.Level.FINEST, Logger.Level.getPredefinedLevel('FINEST'));
    assertEquals(Logger.Level.ALL, Logger.Level.getPredefinedLevel('ALL'));
  },

  testGetPredefinedLevelByValue() {
    assertEquals(
        Logger.Level.OFF, Logger.Level.getPredefinedLevelByValue(Infinity));
    assertEquals(
        Logger.Level.SHOUT, Logger.Level.getPredefinedLevelByValue(1300));
    assertEquals(
        Logger.Level.SHOUT, Logger.Level.getPredefinedLevelByValue(1200));
    assertEquals(
        Logger.Level.SEVERE, Logger.Level.getPredefinedLevelByValue(1150));
    assertEquals(
        Logger.Level.SEVERE, Logger.Level.getPredefinedLevelByValue(1000));
    assertEquals(
        Logger.Level.WARNING, Logger.Level.getPredefinedLevelByValue(900));
    assertEquals(
        Logger.Level.INFO, Logger.Level.getPredefinedLevelByValue(800));
    assertEquals(
        Logger.Level.CONFIG, Logger.Level.getPredefinedLevelByValue(701));
    assertEquals(
        Logger.Level.CONFIG, Logger.Level.getPredefinedLevelByValue(700));
    assertEquals(
        Logger.Level.FINE, Logger.Level.getPredefinedLevelByValue(500));
    assertEquals(
        Logger.Level.FINER, Logger.Level.getPredefinedLevelByValue(400));
    assertEquals(
        Logger.Level.FINEST, Logger.Level.getPredefinedLevelByValue(300));
    assertEquals(Logger.Level.ALL, Logger.Level.getPredefinedLevelByValue(0));
    assertNull(Logger.Level.getPredefinedLevelByValue(-1));
  },

  testGetLogRecord() {
    const name = 'test.get.log.record';
    const level = 1;
    const msg = 'msg';

    const logger = Logger.getLogger(name);
    const logRecord = logger.getLogRecord(level, msg);

    assertEquals(name, logRecord.getLoggerName());
    assertEquals(level, logRecord.getLevel());
    assertEquals(msg, logRecord.getMessage());

    assertNull(logRecord.getException());
  },

  testGetLogRecordWithException() {
    const name = 'test.get.log.record';
    const level = 1;
    const msg = 'msg';
    const ex = Error('Hi');

    const logger = Logger.getLogger(name);
    const logRecord = logger.getLogRecord(level, msg, ex);

    assertEquals(name, logRecord.getLoggerName());
    assertEquals(level, logRecord.getLevel());
    assertEquals(msg, logRecord.getMessage());
    assertEquals(ex, logRecord.getException());
  },
});
