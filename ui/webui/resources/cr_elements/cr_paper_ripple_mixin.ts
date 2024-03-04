// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/paper-ripple/paper-ripple.js';

import {assert} from '//resources/js/assert.js';
import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {PaperRippleElement} from '//resources/polymer/v3_0/paper-ripple/paper-ripple.js';

/**
 * `CrPaperRippleMixin` exposes methods to dynamically create a paper-ripple
 * when needed.
 */

type Constructor<T> = new (...args: any[]) => T;

export const CrPaperRippleMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<CrPaperRippleMixinInterface> => {
      class CrPaperRippleMixin extends superClass implements
          CrPaperRippleMixinInterface {
        static get properties() {
          return {
            /**
             * If true, the element will not produce a ripple effect when
             * interacted with via the pointer.
             */
            noink: {type: Boolean},
          };
        }

        noink: boolean = false;
        rippleContainer: HTMLElement|null = null;

        private ripple_: PaperRippleElement|null = null;

        override updated(changedProperties: PropertyValues<this>) {
          super.updated(changedProperties);

          if (changedProperties.has('noink') && this.hasRipple()) {
            assert(this.ripple_);
            this.ripple_.noink = this.noink;
          }
        }

        /**
         * Ensures this element contains a ripple effect. For startup efficiency
         * the ripple effect is dynamically added on demand when needed.
         */
        ensureRipple() {
          if (this.hasRipple()) {
            return;
          }

          this.ripple_ = this.createRipple();
          this.ripple_.noink = this.noink;
          const rippleContainer = this.rippleContainer || this.shadowRoot;
          assert(rippleContainer);
          rippleContainer.appendChild(this.ripple_);
        }

        /**
         * Returns the `<paper-ripple>` element used by this element to create
         * ripple effects. The element's ripple is created on demand, when
         * necessary, and calling this method will force the
         * ripple to be created.
         */
        getRipple() {
          this.ensureRipple();
          assert(this.ripple_);
          return this.ripple_;
        }

        /**
         * Returns true if this element currently contains a ripple effect.
         */
        hasRipple(): boolean {
          return Boolean(this.ripple_);
        }

        /**
         * Create the element's ripple effect via creating a `<paper-ripple>`.
         * Override this method to customize the ripple element.
         */
        createRipple(): PaperRippleElement {
          return document.createElement('paper-ripple');
        }
      }

      return CrPaperRippleMixin;
    };

export interface CrPaperRippleMixinInterface {
  noink: boolean;
  rippleContainer: HTMLElement|null;

  createRipple(): PaperRippleElement;
  ensureRipple(): void;
  getRipple(): PaperRippleElement;
  hasRipple(): boolean;
}
