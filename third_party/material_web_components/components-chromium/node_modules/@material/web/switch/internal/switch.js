/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import '../../focus/md-focus-ring.js';
import '../../ripple/ripple.js';
import { html, isServer, LitElement, nothing } from 'lit';
import { property, query } from 'lit/decorators.js';
import { classMap } from 'lit/directives/class-map.js';
import { mixinDelegatesAria } from '../../internal/aria/delegate.js';
import { afterDispatch, setupDispatchHooks, } from '../../internal/events/dispatch-hooks.js';
import { dispatchActivationClick, isActivationClick, } from '../../internal/events/form-label-activation.js';
import { redispatchEvent } from '../../internal/events/redispatch-event.js';
import { createValidator, getValidityAnchor, mixinConstraintValidation, } from '../../labs/behaviors/constraint-validation.js';
import { mixinElementInternals } from '../../labs/behaviors/element-internals.js';
import { getFormState, getFormValue, mixinFormAssociated, } from '../../labs/behaviors/form-associated.js';
import { CheckboxValidator } from '../../labs/behaviors/validators/checkbox-validator.js';
// Separate variable needed for closure.
const switchBaseClass = mixinDelegatesAria(mixinConstraintValidation(mixinFormAssociated(mixinElementInternals(LitElement))));
/**
 * @fires input {InputEvent} Fired whenever `selected` changes due to user
 * interaction (bubbles and composed).
 * @fires change {Event} Fired whenever `selected` changes due to user
 * interaction (bubbles).
 */
export class Switch extends switchBaseClass {
    constructor() {
        super();
        /**
         * Puts the switch in the selected state and sets the form submission value to
         * the `value` property.
         */
        this.selected = false;
        /**
         * Shows both the selected and deselected icons.
         */
        this.icons = false;
        /**
         * Shows only the selected icon, and not the deselected icon. If `true`,
         * overrides the behavior of the `icons` property.
         */
        this.showOnlySelectedIcon = false;
        /**
         * When true, require the switch to be selected when participating in
         * form submission.
         *
         * https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/checkbox#validation
         */
        this.required = false;
        /**
         * The value associated with this switch on form submission. `null` is
         * submitted when `selected` is `false`.
         */
        this.value = 'on';
        if (isServer) {
            return;
        }
        // This click listener does not currently need dispatch hooks since it does
        // not check `event.defaultPrevented`.
        this.addEventListener('click', (event) => {
            if (!isActivationClick(event) || !this.input) {
                return;
            }
            this.focus();
            dispatchActivationClick(this.input);
        });
        // Add the aria keyboard interaction pattern for switch and the Enter key.
        // See https://www.w3.org/WAI/ARIA/apg/patterns/switch/.
        setupDispatchHooks(this, 'keydown');
        this.addEventListener('keydown', (event) => {
            afterDispatch(event, () => {
                const ignoreEvent = event.defaultPrevented || event.key !== 'Enter';
                if (ignoreEvent || this.disabled || !this.input) {
                    return;
                }
                this.input.click();
            });
        });
    }
    render() {
        return html `
      <div class="switch ${classMap(this.getRenderClasses())}">
        <input
          id="switch"
          class="touch"
          type="checkbox"
          role="switch"
          aria-label=${this.ariaLabel || nothing}
          ?checked=${this.selected}
          ?disabled=${this.disabled}
          ?required=${this.required}
          @input=${this.handleInput}
          @change=${this.handleChange} />

        <md-focus-ring part="focus-ring" for="switch"></md-focus-ring>
        <span class="track"> ${this.renderHandle()} </span>
      </div>
    `;
    }
    getRenderClasses() {
        return {
            'selected': this.selected,
            'unselected': !this.selected,
            'disabled': this.disabled,
        };
    }
    renderHandle() {
        const classes = {
            'with-icon': this.showOnlySelectedIcon ? this.selected : this.icons,
        };
        return html `
      ${this.renderTouchTarget()}
      <span class="handle-container">
        <md-ripple for="switch" ?disabled="${this.disabled}"></md-ripple>
        <span class="handle ${classMap(classes)}">
          ${this.shouldShowIcons() ? this.renderIcons() : html ``}
        </span>
      </span>
    `;
    }
    renderIcons() {
        return html `
      <div class="icons">
        ${this.renderOnIcon()}
        ${this.showOnlySelectedIcon ? html `` : this.renderOffIcon()}
      </div>
    `;
    }
    /**
     * https://fonts.google.com/icons?selected=Material%20Symbols%20Outlined%3Acheck%3AFILL%400%3Bwght%40500%3BGRAD%400%3Bopsz%4024
     */
    renderOnIcon() {
        return html `
      <slot class="icon icon--on" name="on-icon">
        <svg viewBox="0 0 24 24">
          <path
            d="M9.55 18.2 3.65 12.3 5.275 10.675 9.55 14.95 18.725 5.775 20.35 7.4Z" />
        </svg>
      </slot>
    `;
    }
    /**
     * https://fonts.google.com/icons?selected=Material%20Symbols%20Outlined%3Aclose%3AFILL%400%3Bwght%40500%3BGRAD%400%3Bopsz%4024
     */
    renderOffIcon() {
        return html `
      <slot class="icon icon--off" name="off-icon">
        <svg viewBox="0 0 24 24">
          <path
            d="M6.4 19.2 4.8 17.6 10.4 12 4.8 6.4 6.4 4.8 12 10.4 17.6 4.8 19.2 6.4 13.6 12 19.2 17.6 17.6 19.2 12 13.6Z" />
        </svg>
      </slot>
    `;
    }
    renderTouchTarget() {
        return html `<span class="touch"></span>`;
    }
    shouldShowIcons() {
        return this.icons || this.showOnlySelectedIcon;
    }
    handleInput(event) {
        const target = event.target;
        this.selected = target.checked;
        // <input> 'input' event bubbles and is composed, don't re-dispatch it.
    }
    handleChange(event) {
        // <input> 'change' event is not composed, re-dispatch it.
        redispatchEvent(this, event);
    }
    [getFormValue]() {
        return this.selected ? this.value : null;
    }
    [getFormState]() {
        return String(this.selected);
    }
    formResetCallback() {
        // The selected property does not reflect, so the original attribute set by
        // the user is used to determine the default value.
        this.selected = this.hasAttribute('selected');
    }
    formStateRestoreCallback(state) {
        this.selected = state === 'true';
    }
    [createValidator]() {
        return new CheckboxValidator(() => ({
            checked: this.selected,
            required: this.required,
        }));
    }
    [getValidityAnchor]() {
        return this.input;
    }
}
/** @nocollapse */
Switch.shadowRootOptions = {
    mode: 'open',
    delegatesFocus: true,
};
__decorate([
    property({ type: Boolean })
], Switch.prototype, "selected", void 0);
__decorate([
    property({ type: Boolean })
], Switch.prototype, "icons", void 0);
__decorate([
    property({ type: Boolean, attribute: 'show-only-selected-icon' })
], Switch.prototype, "showOnlySelectedIcon", void 0);
__decorate([
    property({ type: Boolean })
], Switch.prototype, "required", void 0);
__decorate([
    property()
], Switch.prototype, "value", void 0);
__decorate([
    query('input')
], Switch.prototype, "input", void 0);
//# sourceMappingURL=switch.js.map