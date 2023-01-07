// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Minimal externs file provided for places in the code that
 * still use JavaScript instead of TypeScript.
 * @externs
 */

/** @interface */
function CrSearchFieldMixinInterface() {}

/** @return {!HTMLInputElement} */
CrSearchFieldMixinInterface.prototype.getSearchInput = function() {};

/** @return {string} */
CrSearchFieldMixinInterface.prototype.getValue = function() {};

/**
 * @constructor
 * @extends {HTMLElement}
 * @implements {CrSearchFieldMixinInterface}
 */
function CrSearchFieldElement() {}
