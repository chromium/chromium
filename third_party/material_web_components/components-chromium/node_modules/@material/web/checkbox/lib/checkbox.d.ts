/**
 * @license
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../focus/focus-ring.js';
import '../../ripple/ripple.js';
import { LitElement, PropertyValues, TemplateResult } from 'lit';
import { getFormValue } from '../../controller/form-controller.js';
/**
 * A checkbox component.
 */
export declare class Checkbox extends LitElement {
    /**
     * @nocollapse
     */
    static formAssociated: boolean;
    /**
     * Whether or not the checkbox is selected.
     */
    checked: boolean;
    /**
     * Whether or not the checkbox is disabled.
     */
    disabled: boolean;
    /**
     * Whether or not the checkbox is invalid.
     */
    error: boolean;
    /**
     * Whether or not the checkbox is indeterminate.
     *
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/checkbox#indeterminate_state_checkboxes
     */
    indeterminate: boolean;
    /**
     * The value of the checkbox that is submitted with a form when selected.
     *
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/checkbox#value
     */
    value: string;
    /**
     * The HTML name to use in form submission.
     */
    name: string;
    /**
     * The associated form element with which this element's value will submit.
     */
    get form(): HTMLFormElement;
    ariaLabel: string;
    private prevChecked;
    private prevDisabled;
    private prevIndeterminate;
    private readonly ripple;
    private readonly input;
    private showFocusRing;
    private showRipple;
    constructor();
    focus(): void;
    [getFormValue](): string;
    protected update(changed: PropertyValues<Checkbox>): void;
    protected render(): TemplateResult;
    private handleBlur;
    private handleChange;
    private handleFocus;
    private handlePointerDown;
    private readonly getRipple;
    private readonly renderRipple;
}
