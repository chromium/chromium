/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement, PropertyValues } from 'lit';
import { StaticValue } from 'lit/static-html.js';
import { createValidator, getValidityAnchor } from '../../labs/behaviors/constraint-validation.js';
import { getFormValue } from '../../labs/behaviors/form-associated.js';
import { onReportValidity } from '../../labs/behaviors/on-report-validity.js';
import { Validator } from '../../labs/behaviors/validators/validator.js';
/**
 * Input types that are compatible with the text field.
 */
export type TextFieldType = 'email' | 'number' | 'password' | 'search' | 'tel' | 'text' | 'url' | 'textarea';
/**
 * Input types that are not fully supported for the text field.
 */
export type UnsupportedTextFieldType = 'color' | 'date' | 'datetime-local' | 'file' | 'month' | 'time' | 'week';
/**
 * Input types that are incompatible with the text field.
 */
export type InvalidTextFieldType = 'button' | 'checkbox' | 'hidden' | 'image' | 'radio' | 'range' | 'reset' | 'submit';
declare const textFieldBaseClass: import("../../labs/behaviors/mixin.js").MixinReturn<import("../../labs/behaviors/mixin.js").MixinReturn<import("../../labs/behaviors/mixin.js").MixinReturn<import("../../labs/behaviors/mixin.js").MixinReturn<(abstract new (...args: any[]) => import("../../labs/behaviors/element-internals.js").WithElementInternals) & typeof LitElement & import("../../labs/behaviors/form-associated.js").FormAssociatedConstructor, import("../../labs/behaviors/form-associated.js").FormAssociated>, import("../../labs/behaviors/constraint-validation.js").ConstraintValidation>, import("../../labs/behaviors/on-report-validity.js").OnReportValidity>>;
/**
 * A text field component.
 *
 * @fires select {Event} The native `select` event on
 * [`<input>`](https://developer.mozilla.org/en-US/docs/Web/API/HTMLInputElement/select_event)
 * --bubbles
 * @fires change {Event} The native `change` event on
 * [`<input>`](https://developer.mozilla.org/en-US/docs/Web/API/HTMLElement/change_event)
 * --bubbles
 * @fires input {InputEvent} The native `input` event on
 * [`<input>`](https://developer.mozilla.org/en-US/docs/Web/API/HTMLElement/input_event)
 * --bubbles --composed
 */
export declare abstract class TextField extends textFieldBaseClass {
    /** @nocollapse */
    static shadowRootOptions: ShadowRootInit;
    /**
     * Gets or sets whether or not the text field is in a visually invalid state.
     *
     * This error state overrides the error state controlled by
     * `reportValidity()`.
     */
    error: boolean;
    /**
     * The error message that replaces supporting text when `error` is true. If
     * `errorText` is an empty string, then the supporting text will continue to
     * show.
     *
     * This error message overrides the error message displayed by
     * `reportValidity()`.
     */
    errorText: string;
    /**
     * The floating Material label of the textfield component. It informs the user
     * about what information is requested for a text field. It is aligned with
     * the input text, is always visible, and it floats when focused or when text
     * is entered into the textfield. This label also sets accessibilty labels,
     * but the accessible label is overriden by `aria-label`.
     *
     * Learn more about floating labels from the Material Design guidelines:
     * https://m3.material.io/components/text-fields/guidelines
     */
    label: string;
    /**
     * Disables the asterisk on the floating label, when the text field is
     * required.
     */
    noAsterisk: boolean;
    /**
     * Indicates that the user must specify a value for the input before the
     * owning form can be submitted and will render an error state when
     * `reportValidity()` is invoked when value is empty. Additionally the
     * floating label will render an asterisk `"*"` when true.
     *
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Attributes/required
     */
    required: boolean;
    /**
     * The current value of the text field. It is always a string.
     */
    value: string;
    /**
     * An optional prefix to display before the input value.
     */
    prefixText: string;
    /**
     * An optional suffix to display after the input value.
     */
    suffixText: string;
    /**
     * Whether or not the text field has a leading icon. Used for SSR.
     */
    hasLeadingIcon: boolean;
    /**
     * Whether or not the text field has a trailing icon. Used for SSR.
     */
    hasTrailingIcon: boolean;
    /**
     * Conveys additional information below the text field, such as how it should
     * be used.
     */
    supportingText: string;
    /**
     * Override the input text CSS `direction`. Useful for RTL languages that use
     * LTR notation for fractions.
     */
    textDirection: string;
    /**
     * The number of rows to display for a `type="textarea"` text field.
     * Defaults to 2.
     */
    rows: number;
    /**
     * The number of cols to display for a `type="textarea"` text field.
     * Defaults to 20.
     */
    cols: number;
    inputMode: string;
    /**
     * Defines the greatest value in the range of permitted values.
     *
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input#max
     */
    max: string;
    /**
     * The maximum number of characters a user can enter into the text field. Set
     * to -1 for none.
     *
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input#maxlength
     */
    maxLength: number;
    /**
     * Defines the most negative value in the range of permitted values.
     *
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input#min
     */
    min: string;
    /**
     * The minimum number of characters a user can enter into the text field. Set
     * to -1 for none.
     *
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input#minlength
     */
    minLength: number;
    /**
     * When true, hide the spinner for `type="number"` text fields.
     */
    noSpinner: boolean;
    /**
     * A regular expression that the text field's value must match to pass
     * constraint validation.
     *
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input#pattern
     */
    pattern: string;
    /**
     * Defines the text displayed in the textfield when it has no value. Provides
     * a brief hint to the user as to the expected type of data that should be
     * entered into the control. Unlike `label`, the placeholder is not visible
     * and does not float when the textfield has a value.
     *
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Attributes/placeholder
     */
    placeholder: string;
    /**
     * Indicates whether or not a user should be able to edit the text field's
     * value.
     *
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input#readonly
     */
    readOnly: boolean;
    /**
     * Indicates that input accepts multiple email addresses.
     *
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/email#multiple
     */
    multiple: boolean;
    /**
     * Gets or sets the direction in which selection occurred.
     */
    get selectionDirection(): 'forward' | 'backward' | 'none' | null;
    set selectionDirection(value: 'forward' | 'backward' | 'none' | null);
    /**
     * Gets or sets the end position or offset of a text selection.
     */
    get selectionEnd(): number | null;
    set selectionEnd(value: number | null);
    /**
     * Gets or sets the starting position or offset of a text selection.
     */
    get selectionStart(): number | null;
    set selectionStart(value: number | null);
    /**
     * Returns or sets the element's step attribute, which works with min and max
     * to limit the increments at which a numeric or date-time value can be set.
     *
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input#step
     */
    step: string;
    /**
     * The `<input>` type to use, defaults to "text". The type greatly changes how
     * the text field behaves.
     *
     * Text fields support a limited number of `<input>` types:
     *
     * - text
     * - textarea
     * - email
     * - number
     * - password
     * - search
     * - tel
     * - url
     *
     * See
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input#input_types
     * for more details on each input type.
     */
    type: TextFieldType | UnsupportedTextFieldType;
    /**
     * Describes what, if any, type of autocomplete functionality the input
     * should provide.
     *
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Attributes/autocomplete
     */
    autocomplete: string;
    /**
     * The text field's value as a number.
     */
    get valueAsNumber(): number;
    set valueAsNumber(value: number);
    /**
     * The text field's value as a Date.
     */
    get valueAsDate(): Date | null;
    set valueAsDate(value: Date | null);
    protected abstract readonly fieldTag: StaticValue;
    /**
     * Returns true when the text field has been interacted with. Native
     * validation errors only display in response to user interactions.
     */
    private dirty;
    private focused;
    /**
     * Whether or not a native error has been reported via `reportValidity()`.
     */
    private nativeError;
    /**
     * The validation message displayed from a native error via
     * `reportValidity()`.
     */
    private nativeErrorText;
    private get hasError();
    private readonly inputOrTextarea;
    private readonly field;
    private readonly leadingIcons;
    private readonly trailingIcons;
    /**
     * Selects all the text in the text field.
     *
     * https://developer.mozilla.org/en-US/docs/Web/API/HTMLInputElement/select
     */
    select(): void;
    /**
     * Replaces a range of text with a new string.
     *
     * https://developer.mozilla.org/en-US/docs/Web/API/HTMLInputElement/setRangeText
     */
    setRangeText(replacement: string): void;
    setRangeText(replacement: string, start: number, end: number, selectionMode?: SelectionMode): void;
    /**
     * Sets the start and end positions of a selection in the text field.
     *
     * https://developer.mozilla.org/en-US/docs/Web/API/HTMLInputElement/setSelectionRange
     *
     * @param start The offset into the text field for the start of the selection.
     * @param end The offset into the text field for the end of the selection.
     * @param direction The direction in which the selection is performed.
     */
    setSelectionRange(start: number | null, end: number | null, direction?: 'forward' | 'backward' | 'none'): void;
    /**
     * Decrements the value of a numeric type text field by `step` or `n` `step`
     * number of times.
     *
     * https://developer.mozilla.org/en-US/docs/Web/API/HTMLInputElement/stepDown
     *
     * @param stepDecrement The number of steps to decrement, defaults to 1.
     */
    stepDown(stepDecrement?: number): void;
    /**
     * Increments the value of a numeric type text field by `step` or `n` `step`
     * number of times.
     *
     * https://developer.mozilla.org/en-US/docs/Web/API/HTMLInputElement/stepUp
     *
     * @param stepIncrement The number of steps to increment, defaults to 1.
     */
    stepUp(stepIncrement?: number): void;
    /**
     * Reset the text field to its default value.
     */
    reset(): void;
    attributeChangedCallback(attribute: string, newValue: string | null, oldValue: string | null): void;
    protected render(): import("lit-html").TemplateResult<1>;
    protected updated(changedProperties: PropertyValues): void;
    private renderField;
    private renderLeadingIcon;
    private renderTrailingIcon;
    private renderInputOrTextarea;
    private renderPrefix;
    private renderSuffix;
    private renderAffix;
    private getErrorText;
    private handleFocusChange;
    private handleInput;
    private redispatchEvent;
    private getInputOrTextarea;
    private getInput;
    private handleIconChange;
    disabled: boolean;
    name: string;
    [getFormValue](): string;
    formResetCallback(): void;
    formStateRestoreCallback(state: string): void;
    focus(): void;
    [createValidator](): Validator<unknown>;
    [getValidityAnchor](): HTMLElement | null;
    [onReportValidity](invalidEvent: Event | null): void;
}
export {};
