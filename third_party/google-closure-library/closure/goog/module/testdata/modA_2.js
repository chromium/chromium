/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview File #2 of module A.
 */

goog.provide('goog.module.testdata.modA_2');

goog.setTestOnly('goog.module.testdata.modA_2');

goog.require('goog.module.ModuleManager');

goog.module.ModuleManager.getInstance().beforeLoadModuleCode('modA');

if (window.modA2Loaded) throw new Error('modA_2 loaded twice');
window.modA2Loaded = true;

goog.module.ModuleManager.getInstance().setLoaded();
