/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview File #1 of module A.
 */

goog.provide('goog.module.testdata.modA_1');


goog.setTestOnly('goog.module.testdata.modA_1');

if (window.modA1Loaded) throw new Error('modA_1 loaded twice');
window.modA1Loaded = true;
