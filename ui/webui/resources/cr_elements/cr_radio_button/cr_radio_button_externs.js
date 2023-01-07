// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Minimal externs file provided for places in the code that
 * still use JavaScript instead of TypeScript.
 * @externs
 */

/** @interface */
class CrRadioButtonMixinInterface {
  constructor() {
    /** @type {boolean} */
    this.checked;

    /** @type {boolean} */
    this.disabled;

    /** @type {boolean} */
    this.focusabled;

    /** @type {string} */
    this.name;
  }

  /** @return {HTMLElement} */
  getPaperRipple() {}
}

/**
 * @constructor
 * @extends {HTMLElement}
 * @implements {CrRadioButtonMixinInterface}
 */
function CrRadioButtonElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 * @implements {CrRadioButtonMixinInterface}
 */
function CrCardRadioButtonElement() {}
