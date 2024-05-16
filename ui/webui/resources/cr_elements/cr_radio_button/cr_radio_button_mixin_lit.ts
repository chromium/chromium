// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Mixin for cr-radio-button-like elements.
 */

// clang-format off
import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrRippleElement} from '../cr_ripple/cr_ripple.js';

import {assert, assertNotReached} from '//resources/js/assert.js';
// clang-format on

type Constructor<T> = new (...args: any[]) => T;

export const CrRadioButtonMixinLit =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<CrRadioButtonMixinLitInterface> => {
      class CrRadioButtonMixinLit extends superClass implements
          CrRadioButtonMixinLitInterface {
        static get properties() {
          return {
            checked: {
              type: Boolean,
              reflect: true,
            },

            disabled: {
              type: Boolean,
              reflect: true,
              notify: true,
            },

            /**
             * Whether the radio button should be focusable or not. Toggling
             * this property sets the corresponding tabindex of the button
             * itself as well as any links in the button description.
             */
            focusable: {
              type: Boolean,
            },

            hideLabelText: {
              type: Boolean,
              reflect: true,
            },

            label: {
              type: String,
            },

            name: {
              type: String,
              notify: true,
              reflect: true,
            },

            /**
             * Holds the tabIndex for the radio button.
             */
            ariaCheckedString: {type: String},
            ariaDisabledString: {type: String},
          };
        }

        checked: boolean = false;
        disabled: boolean = false;
        focusable: boolean = false;
        hideLabelText: boolean = false;
        label: string = '';
        name?: string;
        ariaCheckedString: string = 'false';
        ariaDisabledString: string = 'false';

        override connectedCallback() {
          super.connectedCallback();
          this.addEventListener('blur', this.hideRipple_.bind(this));
          this.addEventListener('up', this.hideRipple_.bind(this));
        }

        override updated(changedProperties: PropertyValues<this>) {
          super.updated(changedProperties);

          if (changedProperties.has('focusable')) {
            const links = this.querySelectorAll('a');
            links.forEach(link => {
              // Remove the tab stop on any links when the row is unchecked.
              // Since the row is not tabbable, any links within the row
              // should not be either.
              link.tabIndex = this.checked ? 0 : -1;
            });
          }
        }

        getAriaDisabled(): string {
          return this.disabled ? 'true' : 'false';
        }

        getAriaChecked(): string {
          return this.checked ? 'true' : 'false';
        }

        getButtonTabIndex(): number {
          return this.focusable ? 0 : -1;
        }

        override focus() {
          const button = this.shadowRoot!.querySelector<HTMLElement>('#button');
          assert(button);
          button.focus();
        }

        getRipple(): CrRippleElement {
          assertNotReached();
        }

        private hideRipple_() {
          this.getRipple().clear();
        }

        /**
         * When shift-tab is pressed, first bring the focus to the host
         * element. This accomplishes 2 things:
         * 1) Host doesn't get focused when the browser moves the focus
         *    backward.
         * 2) focus now escaped the shadow-dom of this element, so that
         *    it'll correctly obey non-zero tabindex ordering of the
         *    containing document.
         */
        onInputKeydown(e: KeyboardEvent) {
          if (e.shiftKey && e.key === 'Tab') {
            this.focus();
          }
        }
      }

      return CrRadioButtonMixinLit;
    };

export interface CrRadioButtonMixinLitInterface {
  checked: boolean;
  disabled: boolean;
  focusable: boolean;
  hideLabelText: boolean;
  label: string;
  name?: string;
  getButtonTabIndex(): number;
  getAriaDisabled(): string;
  getAriaChecked(): string;
  onInputKeydown(e: KeyboardEvent): void;
  getRipple(): CrRippleElement;
}
