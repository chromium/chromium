/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Validator } from './validator.js';
/**
 * Constraint validation for a text field.
 */
export interface TextFieldState {
    /**
     * The input or textarea state to validate.
     */
    state: InputState | TextAreaState;
    /**
     * The `<input>` or `<textarea>` that is rendered on the page.
     *
     * `minlength` and `maxlength` validation do not apply until a user has
     * interacted with the control and the element is internally marked as dirty.
     * This is a spec quirk, the two properties behave differently from other
     * constraint validation.
     *
     * This means we need an actual rendered element instead of a virtual one,
     * since the virtual element will never be marked as dirty.
     *
     * This can be `null` if the element has not yet rendered, and the validator
     * will fall back to virtual elements for other constraint validation
     * properties, which do apply even if the control is not dirty.
     */
    renderedControl: HTMLInputElement | HTMLTextAreaElement | null;
}
/**
 * Constraint validation properties for an `<input>`.
 */
export interface InputState extends SharedInputAndTextAreaState {
    /**
     * The `<input>` type.
     *
     * Not all constraint validation properties apply to every type. See
     * https://developer.mozilla.org/en-US/docs/Web/HTML/Constraint_validation#validation-related_attributes
     * for which properties will apply to which types.
     */
    readonly type: string;
    /**
     * The regex pattern a value must match.
     */
    readonly pattern: string;
    /**
     * The minimum value.
     */
    readonly min: string;
    /**
     * The maximum value.
     */
    readonly max: string;
    /**
     * The step interval of the value.
     */
    readonly step: string;
}
/**
 * Constraint validation properties for a `<textarea>`.
 */
export interface TextAreaState extends SharedInputAndTextAreaState {
    /**
     * The type, must be "textarea" to inform the validator to use `<textarea>`
     * instead of `<input>`.
     */
    readonly type: 'textarea';
}
/**
 * Constraint validation properties shared between an `<input>` and
 * `<textarea>`.
 */
interface SharedInputAndTextAreaState {
    /**
     * The current value.
     */
    readonly value: string;
    /**
     * Whether the textarea is required.
     */
    readonly required: boolean;
    /**
     * The minimum length of the value.
     */
    readonly minLength: number;
    /**
     * The maximum length of the value.
     */
    readonly maxLength: number;
}
/**
 * A validator that provides constraint validation that emulates `<input>` and
 * `<textarea>` validation.
 */
export declare class TextFieldValidator extends Validator<TextFieldState> {
    private inputControl?;
    private textAreaControl?;
    protected computeValidity({ state, renderedControl }: TextFieldState): {
        validity: ValidityState;
        validationMessage: string;
    };
    protected equals({ state: prev }: TextFieldState, { state: next }: TextFieldState): boolean;
    protected copy({ state }: TextFieldState): TextFieldState;
    private copyInput;
    private copyTextArea;
    private copySharedState;
}
export {};
