// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Drop effect names supported as a value of DataTransfer.dropEffect.
 * @enum {string}
 */
const DropEffectType = {
  NONE: 'none',
  COPY: 'copy',
  MOVE: 'move',
  LINK: 'link'
};

/**
 * Represents a drop effect and a label to describe it.
 */
class DropEffectAndLabel {
  /**
   * @param {!DropEffectType} dropEffect
   * @param {?string} label
   */
  constructor(dropEffect, label) {
    /** @private @const {!DropEffectType} */
    this.dropEffect_ = dropEffect;

    /**
     * Optional description why the drop operation is (not) permitted.
     * @private @const {?string}
     */
    this.label_ = label;
  }

  /**
   * @return {!DropEffectType} Returns the type of the drop effect.
   */
  getDropEffect() {
    return this.dropEffect_;
  }

  /**
   * @return {?string} Returns the label. |none| if a label should not appear.
   */
  getLabel() {
    return this.label_;
  }
}
