/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview File #1 of module B.
 */

goog.provide('goog.module.testdata.modB_1');

goog.setTestOnly('goog.module.testdata.modB_1');

goog.require('goog.module.ModuleManager');

goog.module.ModuleManager.getInstance().beforeLoadModuleCode('modB');

function throwErrorInModuleB() {
  throw new Error();
}

if (window.modB1Loaded) throw new Error('modB_1 loaded twice');
window.modB1Loaded = true;

goog.module.ModuleManager.getInstance().setLoaded();
