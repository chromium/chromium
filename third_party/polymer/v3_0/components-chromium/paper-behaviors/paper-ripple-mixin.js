// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '../polymer/polymer_bundled.min.js';
import '../paper-ripple/paper-ripple.js';

import {dedupingMixin, dom} from '../polymer/polymer_bundled.min.js';

/**
 * Note: This file is forked from Polymer's paper-ripple-behavior.js
 *
 * `PaperRippleMixin` dynamically implements a ripple when the element has
 * focus via pointer or keyboard.
 */

export const PaperRippleMixin = dedupingMixin(superClass => {
  class PaperRippleMixin extends superClass {
    static get properties() {
      return {
        /**
         * If true, the element will not produce a ripple effect when interacted
         * with via the pointer.
         */
        noink: {type: Boolean, observer: '_noinkChanged'},

        /**
         * @type {Element|undefined}
         */
        _rippleContainer: Object,
      };
    }


    /**
     * Ensures this element contains a ripple effect. For startup efficiency
     * the ripple effect is dynamically on demand when needed.
     */
    ensureRipple() {
      if (this.hasRipple()) {
        return;
      }

      this._ripple = this._createRipple();
      this._ripple.noink = this.noink;
      var rippleContainer = this._rippleContainer || this.root;
      if (rippleContainer) {
        rippleContainer.appendChild(this._ripple);
      }
    }

    /**
     * Returns the `<paper-ripple>` element used by this element to create
     * ripple effects. The element's ripple is created on demand, when
     * necessary, and calling this method will force the
     * ripple to be created.
     */
    getRipple() {
      this.ensureRipple();
      return this._ripple;
    }

    /**
     * Returns true if this element currently contains a ripple effect.
     * @return {boolean}
     */
    hasRipple() {
      return Boolean(this._ripple);
    }

    /**
     * Create the element's ripple effect via creating a `<paper-ripple>`.
     * Override this method to customize the ripple element.
     * @return {!PaperRippleElement} Returns a `<paper-ripple>` element.
     */
    _createRipple() {
      var element = /** @type {!PaperRippleElement} */ (
          document.createElement('paper-ripple'));
      return element;
    }

    _noinkChanged(noink) {
      if (this.hasRipple()) {
        this._ripple.noink = noink;
      }
    }
  }

  return PaperRippleMixin;
});
