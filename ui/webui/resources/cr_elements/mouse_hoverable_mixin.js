// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file provides a mixin to manage a `hovered` style on mouse
 * events. Relies on listening for pointer events as touch devices may fire
 * mouse events too.
 */

import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @type {string} */
const HOVERED_STYLE = 'hovered';

/** @interface */
export class MouseHoverableMixinInterface {}

/**
 * @polymer
 * @mixinFunction
 */
export const MouseHoverableMixin = dedupingMixin(superClass => {
  /**
   * @polymer
   * @mixinClass
   * @implements {MouseHoverableMixinInterface}
   */
  class MouseHoverableMixin extends superClass {
    ready() {
      super.ready();

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
  }

  return MouseHoverableMixin;
});
