// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Helper functions for implementing an incremental search field. See
 * <settings-subpage-search> for a simple implementation.
 */
import {assertNotReached} from '//resources/js/assert.js';
import type {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CrInputElement} from '../cr_input/cr_input.js';


type Constructor<T> = new (...args: any[]) => T;

export const CrSearchFieldMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<CrSearchFieldMixinInterface> => {
      class CrSearchFieldMixin extends superClass implements
          CrSearchFieldMixinInterface {
        static get properties() {
          return {
            // Prompt text to display in the search field.
            label: {
              type: String,
            },

            // Tooltip to display on the clear search button.
            clearLabel: {
              type: String,
            },

            hasSearchText: {
              type: Boolean,
              reflectToAttribute: true,
            },
          };
        }

        label: string = '';
        clearLabel: string = '';
        hasSearchText: boolean = false;
        private effectiveValue_: string = '';
        private searchDelayTimer_: number = -1;

        /**
         * @return The input field element the behavior should use.
         */
        getSearchInput(): HTMLInputElement|CrInputElement {
          assertNotReached();
        }

        /**
         * @return The value of the search field.
         */
        getValue(): string {
          return this.getSearchInput().value;
        }

        private fire_(eventName: string, detail?: any) {
          this.dispatchEvent(new CustomEvent(
              eventName, {bubbles: true, composed: true, detail}));
        }

        /**
         * Sets the value of the search field.
         * @param noEvent Whether to prevent a 'search-changed' event
         *     firing for this change.
         */
        setValue(value: string, noEvent?: boolean) {
          const updated = this.updateEffectiveValue_(value);
          this.getSearchInput().value = this.effectiveValue_;
          if (!updated) {
            // If the input is only whitespace and value is empty,
            // |hasSearchText| needs to be updated.
            if (value === '' && this.hasSearchText) {
              this.hasSearchText = false;
            }
            return;
          }

          this.onSearchTermInput();
          if (!noEvent) {
            this.fire_('search-changed', this.effectiveValue_);
          }
        }

        private scheduleSearch_() {
          if (this.searchDelayTimer_ >= 0) {
            clearTimeout(this.searchDelayTimer_);
          }
          // Dispatch 'search' event after:
          //    0ms if the value is empty
          //  500ms if the value length is 1
          //  400ms if the value length is 2
          //  300ms if the value length is 3
          //  200ms if the value length is 4 or greater.
          // The logic here was copied from WebKit's native 'search' event.
          const length = this.getValue().length;
          const timeoutMs =
              length > 0 ? (500 - 100 * (Math.min(length, 4) - 1)) : 0;
          this.searchDelayTimer_ = setTimeout(() => {
            this.getSearchInput().dispatchEvent(new CustomEvent(
                'search', {composed: true, detail: this.getValue()}));
            this.searchDelayTimer_ = -1;
          }, timeoutMs);
        }

        onSearchTermSearch() {
          this.onValueChanged_(this.getValue(), false);
        }

        /**
         * Update the state of the search field whenever the underlying input
         * value changes. Unlike onsearch or onkeypress, this is reliably called
         * immediately after any change, whether the result of user input or JS
         * modification.
         */
        onSearchTermInput() {
          this.hasSearchText = this.getSearchInput().value !== '';
          this.scheduleSearch_();
        }

        /**
         * Updates the internal state of the search field based on a change that
         * has already happened.
         * @param noEvent Whether to prevent a 'search-changed' event
         *     firing for this change.
         */
        private onValueChanged_(newValue: string, noEvent: boolean) {
          const updated = this.updateEffectiveValue_(newValue);
          if (updated && !noEvent) {
            this.fire_('search-changed', this.effectiveValue_);
          }
        }

        /**
         * Trim leading whitespace and replace consecutive whitespace with
         * single space. This will prevent empty string searches and searches
         * for effectively the same query.
         */
        private updateEffectiveValue_(value: string): boolean {
          const effectiveValue = value.replace(/\s+/g, ' ').replace(/^\s/, '');
          if (effectiveValue === this.effectiveValue_) {
            return false;
          }

          this.effectiveValue_ = effectiveValue;
          return true;
        }
      }

      return CrSearchFieldMixin;
    });

export interface CrSearchFieldMixinInterface {
  label: string;
  clearLabel: string;
  hasSearchText: boolean;
  getSearchInput(): HTMLInputElement|CrInputElement;
  getValue(): string;
  setValue(value: string, noEvent?: boolean): void;
  onSearchTermSearch(): void;
  onSearchTermInput(): void;
}
