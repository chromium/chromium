/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { property } from 'lit/decorators.js';
/**
 * A property symbol that indicates whether or not a `Focusable` element can be
 * focused.
 */
export const isFocusable = Symbol('isFocusable');
const privateIsFocusable = Symbol('privateIsFocusable');
const externalTabIndex = Symbol('externalTabIndex');
const isUpdatingTabIndex = Symbol('isUpdatingTabIndex');
const updateTabIndex = Symbol('updateTabIndex');
/**
 * Mixes in focusable functionality for a class.
 *
 * Elements can enable and disable their focusability with the `isFocusable`
 * symbol property. Changing `tabIndex` will trigger a lit render, meaning
 * `this.tabIndex` can be used in template expressions.
 *
 * This mixin will preserve externally-set tabindices. If an element turns off
 * focusability, but a user sets `tabindex="0"`, it will still be focusable.
 *
 * To remove user overrides and restore focusability control to the element,
 * remove the `tabindex` attribute.
 *
 * @param base The class to mix functionality into.
 * @return The provided class with `Focusable` mixed in.
 */
export function mixinFocusable(base) {
    var _a, _b, _c;
    class FocusableElement extends base {
        constructor() {
            super(...arguments);
            this[_a] = true;
            this[_b] = null;
            this[_c] = false;
        }
        get [isFocusable]() {
            return this[privateIsFocusable];
        }
        set [isFocusable](value) {
            if (this[isFocusable] === value) {
                return;
            }
            this[privateIsFocusable] = value;
            this[updateTabIndex]();
        }
        connectedCallback() {
            super.connectedCallback();
            this[updateTabIndex]();
        }
        attributeChangedCallback(name, old, value) {
            if (name !== 'tabindex') {
                super.attributeChangedCallback(name, old, value);
                return;
            }
            this.requestUpdate('tabIndex', Number(old ?? -1));
            if (this[isUpdatingTabIndex]) {
                // Not an externally-initiated update.
                return;
            }
            if (!this.hasAttribute('tabindex')) {
                // User removed the attribute, can now use internal tabIndex
                this[externalTabIndex] = null;
                this[updateTabIndex]();
                return;
            }
            this[externalTabIndex] = this.tabIndex;
        }
        [(_a = privateIsFocusable, _b = externalTabIndex, _c = isUpdatingTabIndex, updateTabIndex)]() {
            const internalTabIndex = this[isFocusable] ? 0 : -1;
            const computedTabIndex = this[externalTabIndex] ?? internalTabIndex;
            this[isUpdatingTabIndex] = true;
            this.tabIndex = computedTabIndex;
            this[isUpdatingTabIndex] = false;
        }
    }
    __decorate([
        property({ noAccessor: true })
    ], FocusableElement.prototype, "tabIndex", void 0);
    return FocusableElement;
}
//# sourceMappingURL=focusable.js.map