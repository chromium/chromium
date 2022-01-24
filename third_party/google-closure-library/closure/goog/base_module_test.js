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

/** @fileoverview Unit tests for Closure's base.js's goog.module support. */

goog.module('goog.baseModuleTest');
goog.setTestOnly();

// Used to test dynamic loading works, see testRequire*
const Timer = goog.require('goog.Timer');
const Replacer = goog.require('goog.testing.PropertyReplacer');
const jsunit = goog.require('goog.testing.jsunit');
const testSuite = goog.require('goog.testing.testSuite');

const testModule = goog.require('goog.test_module');

const stubs = new Replacer();

function assertProvideFails(namespace) {
  assertThrows(
      'goog.provide(' + namespace + ') should have failed',
      goog.partial(goog.provide, namespace));
}

function assertModuleFails(namespace) {
  assertThrows(
      'goog.module(' + namespace + ') should have failed',
      goog.partial(goog.module, namespace));
}

function assertLoadModule(msg, moduleDef) {
  assertNotThrows(msg, goog.partial(goog.loadModule, moduleDef));
}

testSuite({
  teardown: function() {
    stubs.reset();
  },

  /** @suppress {missingRequire} reference to fully qualified goog.Timer. */
  testModuleDecl: function() {
    // assert that goog.module doesn't modify the global namespace
    assertNull(
        'module failed to protect global namespace: ' +
            'goog.baseModuleTest',
        goog.getObjectByName('goog.baseModuleTest'));
  },

  testModuleScoping: function() {
    // assert test functions are not exported to the global namespace
    assertNotUndefined('module failed: testModule', testModule);
    assertFalse(
        'module failed: testModule',
        typeof goog.global.testModuleScoping === 'function');
  },

  testProvideStrictness1: function() {
    assertModuleFails('goog.xy');  // not in goog.loadModule

    assertProvideFails('goog.baseModuleTest');  // this file.
  },

  /** @suppress {visibility} */
  testProvideStrictness2: function() {
    // goog.module "provides" a namespace
    assertTrue(goog.isProvided_('goog.baseModuleTest'));
  },

  testExportSymbol: function() {
    // Assert that export symbol works from within a goog.module.
    const date = new Date();

    assertTrue(typeof nodots == 'undefined');
    goog.exportSymbol('nodots', date);
    assertEquals(date, nodots);  // globals are accessible from within a module.
    nodots = undefined;
  },

  testLoadModule: function() {
    assertLoadModule(
        'Loading a module that exports a typedef should succeed',
        'goog.module(\'goog.test_module_typedef\');' +
            'var typedef;' +
            'exports = typedef;');
  },

  //=== tests for Require logic ===
  /** @suppress {missingRequire} reference to fully qualified goog.Timer. */
  testLegacyRequire: function() {
    // goog.Timer is a legacy module loaded above
    assertNotUndefined('goog.Timer should be available', goog.Timer);

    // Verify that a legacy module can be aliases with goog.require
    assertTrue(
        'Timer should be the goog.Timer namespace object',
        goog.Timer === Timer);

    // and its dependencies
    assertNotUndefined(
        'goog.events.EventTarget should be available', goog.events.EventTarget);
  },

  /**
   * @suppress {missingRequire, missingProperties} reference to fully qualified
   * goog.test_module.
   */
  testRequireModule: function() {
    assertEquals(
        'module failed to export legacy namespace: ' +
            'goog.test_module',
        testModule, goog.test_module);
    assertUndefined(
        'module failed to protect global namespace: ' +
            'goog.test_module_dep',
        goog.test_module_dep);

    // The test module is available under its alias
    assertNotUndefined('testModule is loaded', testModule);
    assertTrue('module failed: testModule', typeof testModule === 'function');

    // Test that any escaping of </script> in test files is correct. Escape the
    // / in </script> here so that any such code does not affect it here.
    assertEquals('<\/script>', testModule.CLOSING_SCRIPT_TAG);
  }
});
