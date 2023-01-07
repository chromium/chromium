// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Minimal externs file provided for places in the code that
 * still use JavaScript instead of TypeScript.
 * @externs
 */

/**
 * @typedef {{
 *   value: number,
 *   label: string,
 *   ariaValue: (number|undefined),
 * }}
 */
let SliderTick;

/**
 * @constructor
 * @extends {HTMLElement}
 */
function CrSliderElement() {}

/** @type {number} */
CrSliderElement.prototype.value;

/** @type {!Array<!SliderTick>|!Array<number>} */
CrSliderElement.prototype.ticks;
