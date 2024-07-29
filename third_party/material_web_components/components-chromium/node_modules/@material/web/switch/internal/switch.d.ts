/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../focus/md-focus-ring.js';
import '../../ripple/ripple.js';
import { LitElement, TemplateResult } from 'lit';
import { createValidator, getValidityAnchor } from '../../labs/behaviors/constraint-validation.js';
import { getFormState, getFormValue } from '../../labs/behaviors/form-associated.js';
import { CheckboxValidator } from '../../labs/behaviors/validators/checkbox-validator.js';
declare const switchBaseClass: import("../../labs/behaviors/mixin.js").MixinReturn<import("../../labs/behaviors/mixin.js").MixinReturn<import("../../labs/behaviors/mixin.js").MixinReturn<(abstract new (...args: any[]) => import("../../labs/behaviors/element-internals.js").WithElementInternals) & typeof LitElement & import("../../labs/behaviors/form-associated.js").FormAssociatedConstructor, import("../../labs/behaviors/form-associated.js").FormAssociated>, import("../../labs/behaviors/constraint-validation.js").ConstraintValidation>>;
/**
 * @fires input {InputEvent} Fired whenever `selected` changes due to user
 * interaction (bubbles and composed).
 * @fires change {Event} Fired whenever `selected` changes due to user
 * interaction (bubbles).
 */
export declare class Switch extends switchBaseClass {
    /** @nocollapse */
    static shadowRootOptions: ShadowRootInit;
    /**
     * Puts the switch in the selected state and sets the form submission value to
     * the `value` property.
     */
    selected: boolean;
    /**
     * Shows both the selected and deselected icons.
     */
    icons: boolean;
    /**
     * Shows only the selected icon, and not the deselected icon. If `true`,
     * overrides the behavior of the `icons` property.
     */
    showOnlySelectedIcon: boolean;
    /**
     * When true, require the switch to be selected when participating in
     * form submission.
     *
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/checkbox#validation
     */
    required: boolean;
    /**
     * The value associated with this switch on form submission. `null` is
     * submitted when `selected` is `false`.
     */
    value: string;
    private readonly input;
    constructor();
    protected render(): TemplateResult;
    private getRenderClasses;
    private renderHandle;
    private renderIcons;
    /**
     * https://fonts.google.com/icons?selected=Material%20Symbols%20Outlined%3Acheck%3AFILL%400%3Bwght%40500%3BGRAD%400%3Bopsz%4024
     */
    private renderOnIcon;
    /**
     * https://fonts.google.com/icons?selected=Material%20Symbols%20Outlined%3Aclose%3AFILL%400%3Bwght%40500%3BGRAD%400%3Bopsz%4024
     */
    private renderOffIcon;
    private renderTouchTarget;
    private shouldShowIcons;
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
