/**
 * @license
 * Copyright 2018 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../focus/md-focus-ring.js';
import '../../ripple/ripple.js';
import { LitElement } from 'lit';
import { createValidator, getValidityAnchor } from '../../labs/behaviors/constraint-validation.js';
import { getFormState, getFormValue } from '../../labs/behaviors/form-associated.js';
import { RadioValidator } from '../../labs/behaviors/validators/radio-validator.js';
declare const CHECKED: unique symbol;
declare const radioBaseClass: import("../../labs/behaviors/mixin.js").MixinReturn<import("../../labs/behaviors/mixin.js").MixinReturn<(abstract new (...args: any[]) => import("../../labs/behaviors/element-internals.js").WithElementInternals) & (abstract new (...args: any[]) => import("../../labs/behaviors/focusable.js").Focusable) & typeof LitElement & import("../../labs/behaviors/form-associated.js").FormAssociatedConstructor, import("../../labs/behaviors/form-associated.js").FormAssociated>, import("../../labs/behaviors/constraint-validation.js").ConstraintValidation>;
/**
 * A radio component.
 *
 * @fires input {InputEvent} Dispatched when the value changes from user
 * interaction. --bubbles
 * @fires change {Event} Dispatched when the value changes from user
 * interaction. --bubbles --composed
 */
export declare class Radio extends radioBaseClass {
    private readonly maskId;
    /**
     * Whether or not the radio is selected.
     */
    get checked(): boolean;
    set checked(checked: boolean);
    [CHECKED]: boolean;
    /**
     * Whether or not the radio is required. If any radio is required in a group,
     * all radios are implicitly required.
     */
    required: boolean;
    /**
     * The element value to use in form submission when checked.
     */
    value: string;
    private readonly container;
    private readonly selectionController;
    constructor();
    protected render(): import("lit-html").TemplateResult<1>;
    protected updated(): void;
    private handleClick;
    private handleKeydown;
    disabled: boolean;
    name: string;
    [getFormValue](): string;
    [getFormState](): string;
    formResetCallback(): void;
    formStateRestoreCallback(state: string): void;
    [createValidator](): RadioValidator;
    [getValidityAnchor](): HTMLElement;
}
export {};
