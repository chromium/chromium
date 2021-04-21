// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file provides a mixin and a class to manage a `hovered`
 * style on mouse events. Relies on listening for pointer events as touch
 * devices may fire mouse events too.
 */

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @type {string} */
const HOVERED_STYLE = 'hovered';

/** @interface */
class AttachableInterface {
  attach() {}
}

export const MouseHoverableElementMixin = (superClass) =>
    class extends superClass {
  attach() {
    this.addEventListener('pointerenter', (e) => {
      const hostElement = /** @type {!Element} */ (e.currentTarget);
      hostElement.classList.toggle(HOVERED_STYLE, e.pointerType === 'mouse');
    });

    this.addEventListener('pointerleave', (e) => {
      if (e.pointerType !== 'mouse') {
        return;
      }

      const hostElement = /** @type {!Element} */ (e.currentTarget);
      hostElement.classList.remove(HOVERED_STYLE);
    });
  }
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {AttachableInterface}
 */
const MouseHoverablePolymerElement = MouseHoverableElementMixin(PolymerElement);

/** @polymer */
export class MouseHoverableElement extends MouseHoverablePolymerElement {
  /** @override */
  ready() {
    super.ready();
    this.attach();
  }
}
