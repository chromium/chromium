// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Minimal externs file provided for places in the code that
 * still use JavaScript instead of TypeScript.
 * @externs
 */

/** @interface */
function CrSearchFieldBehaviorInterface() {}

/**
 * @param {string} value
 * @param {boolean=} noEvent
 */
CrSearchFieldBehaviorInterface.prototype.setValue = function(value, noEvent) {};

/**
 * @constructor
 * @extends {HTMLElement}
 * @implements {CrSearchFieldBehaviorInterface}
 */
function CrToolbarSearchFieldElement() {}

/** @return {!HTMLInputElement} */
CrToolbarSearchFieldElement.prototype.getSearchInput = function() {};

CrToolbarSearchFieldElement.prototype.showAndFocus = function() {};

/** @return {boolean} */
CrToolbarSearchFieldElement.prototype.isSearchFocused = function() {};
