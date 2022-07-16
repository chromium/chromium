/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

// Mock JScript functions
goog.module('goog.userAgent.jscriptTest');
goog.setTestOnly();

const jscript = goog.require('goog.userAgent.jscript');
const testSuite = goog.require('goog.testing.testSuite');



globalThis['ScriptEngine'] = function() {
  return 'JScript';
};

globalThis['ScriptEngineMajorVersion'] = function() {
  return 1;
};

globalThis['ScriptEngineMinorVersion'] = function() {
  return 2;
};

globalThis['ScriptEngineBuildVersion'] = function() {
  return 3456;
};


testSuite({
  setUpPage() {
    jscript.init();
  },

  testHasJscript() {
    assertTrue('Should have jscript', jscript.HAS_JSCRIPT);
  },

  testVersion() {
    assertEquals('Version should be 1.2.3456', '1.2.3456', jscript.VERSION);
  },

  testIsVersion() {
    assertTrue(
        'Should be version 1.2.3456 or larger', jscript.isVersion('1.2.3456'));
    assertTrue('Should be version 1.2 or larger', jscript.isVersion('1.2'));
    assertFalse(
        'Should not be version 8.9 or larger', jscript.isVersion('8.9'));
  },
});
