/**
 * @license
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../focus/md-focus-ring.js';
import '../../ripple/ripple.js';
import { LitElement, PropertyValues } from 'lit';
import { createValidator, getValidityAnchor } from '../../labs/behaviors/constraint-validation.js';
import { getFormState, getFormValue } from '../../labs/behaviors/form-associated.js';
import { CheckboxValidator } from '../../labs/behaviors/validators/checkbox-validator.js';
declare const checkboxBaseClass: import("../../labs/behaviors/mixin.js").MixinReturn<import("../../labs/behaviors/mixin.js").MixinReturn<import("../../labs/behaviors/mixin.js").MixinReturn<(abstract new (...args: any[]) => import("../../labs/behaviors/element-internals.js").WithElementInternals) & typeof LitElement & import("../../labs/behaviors/form-associated.js").FormAssociatedConstructor, import("../../labs/behaviors/form-associated.js").FormAssociated>, import("../../labs/behaviors/constraint-validation.js").ConstraintValidation>>;
/**
 * A checkbox component.
 *
 *
 * @fires change {Event} The native `change` event on
 * [`<input>`](https://developer.mozilla.org/en-US/docs/Web/API/HTMLElement/change_event)
 * --bubbles
 * @fires input {InputEvent} The native `input` event on
 * [`<input>`](https://developer.mozilla.org/en-US/docs/Web/API/HTMLElement/input_event)
 * --bubbles --composed
 */
export declare class Checkbox extends checkboxBaseClass {
    /** @nocollapse */
    static shadowRootOptions: {
        delegatesFocus: boolean;
        mode: ShadowRootMode;
        slotAssignment?: SlotAssignmentMode;
    };
    /**
     * Whether or not the checkbox is selected.
     */
    checked: boolean;
    /**
     * Whether or not the checkbox is indeterminate.
     *
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/checkbox#indeterminate_state_checkboxes
     */
    indeterminate: boolean;
    /**
     * When true, require the checkbox to be selected when participating in
     * form submission.
     *
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/checkbox#validation
     */
    required: boolean;
    /**
     * The value of the checkbox that is submitted with a form when selected.
     *
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/checkbox#value
     */
    value: string;
    private prevChecked;
    private prevDisabled;
    private prevIndeterminate;
    private readonly input;
    constructor();
    protected update(changed: PropertyValues<Checkbox>): void;
    protected render(): import("lit-html").TemplateResult<1>;
    private handleInput;
    private handleChange;
    disabled: boolean;
    name: string;
    [getFormValue](): string;
    [getFormState](): string;
    formResetCallback(): void;
    formStateRestoreCallback(state: string): void;
    [createValidator](): CheckboxValidator;
    [getValidityAnchor](): HTMLInputElement;
}
export {};
