/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview This shim namespace defines the shared
 * mechanism variables used in mechanismSeparationTester
 * and mechanismSelectionTester. This exists to allow test compilation
 * to work correctly for these legacy tests.
 */

goog.provide('goog.storage.mechanism.mechanismTestDefinition');
goog.setTestOnly('goog.storage.mechanism.mechanismTestDefinition');

var mechanism;
var mechanism_shared;
var mechanism_separate;
var minimumQuota;
