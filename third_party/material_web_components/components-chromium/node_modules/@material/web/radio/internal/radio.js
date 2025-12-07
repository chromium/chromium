/**
 * @license
 * Copyright 2018 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
var _a;
import { __decorate } from "tslib";
import '../../focus/md-focus-ring.js';
import '../../ripple/ripple.js';
import { html, isServer, LitElement } from 'lit';
import { property, query } from 'lit/decorators.js';
import { classMap } from 'lit/directives/class-map.js';
import { isActivationClick } from '../../internal/events/form-label-activation.js';
import { createValidator, getValidityAnchor, mixinConstraintValidation, } from '../../labs/behaviors/constraint-validation.js';
import { internals, mixinElementInternals, } from '../../labs/behaviors/element-internals.js';
import { mixinFocusable } from '../../labs/behaviors/focusable.js';
import { getFormState, getFormValue, mixinFormAssociated, } from '../../labs/behaviors/form-associated.js';
import { RadioValidator } from '../../labs/behaviors/validators/radio-validator.js';
import { SingleSelectionController } from './single-selection-controller.js';
const CHECKED = Symbol('checked');
let maskId = 0;
// Separate variable needed for closure.
const radioBaseClass = mixinConstraintValidation(mixinFormAssociated(mixinElementInternals(mixinFocusable(LitElement))));
/**
 * A radio component.
 *
 * @fires input {InputEvent} Dispatched when the value changes from user
 * interaction. --bubbles
 * @fires change {Event} Dispatched when the value changes from user
 * interaction. --bubbles --composed
 */
export class Radio extends radioBaseClass {
    /**
     * Whether or not the radio is selected.
     */
    get checked() {
        return this[CHECKED];
    }
    set checked(checked) {
        const wasChecked = this.checked;
        if (wasChecked === checked) {
            return;
        }
        this[CHECKED] = checked;
        this.requestUpdate('checked', wasChecked);
        this.selectionController.handleCheckedChange();
    }
    constructor() {
        super();
        // Unique maskId is required because of a Safari bug that fail to persist
        // reference to the mask. This should be removed once the bug is fixed.
        this.maskId = `cutout${++maskId}`;
        this[_a] = false;
        /**
         * Whether or not the radio is required. If any radio is required in a group,
         * all radios are implicitly required.
         */
        this.required = false;
        /**
         * The element value to use in form submission when checked.
         */
        this.value = 'on';
        this.selectionController = new SingleSelectionController(this);
        this.addController(this.selectionController);
        if (!isServer) {
            this[internals].role = 'radio';
            this.addEventListener('click', this.handleClick.bind(this));
            this.addEventListener('keydown', this.handleKeydown.bind(this));
        }
    }
    render() {
        const classes = { 'checked': this.checked };
        return html `
      <div class="container ${classMap(classes)}" aria-hidden="true">
        <md-ripple
          part="ripple"
          .control=${this}
          ?disabled=${this.disabled}></md-ripple>
        <md-focus-ring part="focus-ring" .control=${this}></md-focus-ring>
        <svg class="icon" viewBox="0 0 20 20">
          <mask id="${this.maskId}">
            <rect width="100%" height="100%" fill="white" />
            <circle cx="10" cy="10" r="8" fill="black" />
          </mask>
          <circle
            class="outer circle"
            cx="10"
            cy="10"
            r="10"
            mask="url(#${this.maskId})" />
          <circle class="inner circle" cx="10" cy="10" r="5" />
        </svg>

        <div class="touch-target"></div>
      </div>
    `;
    }
    updated() {
        this[internals].ariaChecked = String(this.checked);
    }
    async handleClick(event) {
        if (this.disabled) {
            return;
        }
        // allow event to propagate to user code after a microtask.
        await 0;
        if (event.defaultPrevented) {
            return;
        }
        if (isActivationClick(event)) {
            this.focus();
        }
        // Per spec, clicking on a radio input always selects it.
        this.checked = true;
        this.dispatchEvent(new Event('change', { bubbles: true }));
        this.dispatchEvent(new InputEvent('input', { bubbles: true, composed: true }));
    }
    async handleKeydown(event) {
        // allow event to propagate to user code after a microtask.
        await 0;
        if (event.key !== ' ' || event.defaultPrevented) {
            return;
        }
        this.click();
    }
    [(_a = CHECKED, getFormValue)]() {
        return this.checked ? this.value : null;
    }
    [getFormState]() {
        return String(this.checked);
    }
    formResetCallback() {
        // The checked property does not reflect, so the original attribute set by
        // the user is used to determine the default value.
        this.checked = this.hasAttribute('checked');
    }
    formStateRestoreCallback(state) {
        this.checked = state === 'true';
    }
    [createValidator]() {
        return new RadioValidator(() => {
            if (!this.selectionController) {
                // Validation runs on superclass construction, so selection controller
                // might not actually be ready until this class constructs.
                return [this];
            }
            return this.selectionController.controls;
        });
    }
    [getValidityAnchor]() {
        return this.container;
    }
}
__decorate([
    property({ type: Boolean })
], Radio.prototype, "checked", null);
__decorate([
    property({ type: Boolean })
], Radio.prototype, "required", void 0);
__decorate([
    property()
], Radio.prototype, "value", void 0);
__decorate([
    query('.container')
], Radio.prototype, "container", void 0);
//# sourceMappingURL=radio.js.map