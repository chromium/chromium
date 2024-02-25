/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement } from 'lit';
import { WithElementInternals } from './element-internals.js';
import { FormAssociated } from './form-associated.js';
import { MixinBase, MixinReturn } from './mixin.js';
import { Validator } from './validators/validator.js';
/**
 * A form associated element that provides constraint validation APIs.
 *
 * https://developer.mozilla.org/en-US/docs/Web/HTML/Constraint_validation
 */
export interface ConstraintValidation extends FormAssociated {
    /**
     * Returns a ValidityState object that represents the validity states of the
     * element.
     *
     * https://developer.mozilla.org/en-US/docs/Web/API/ValidityState
     */
    readonly validity: ValidityState;
    /**
     * Returns a validation error message or an empty string if the element is
     * valid.
     *
     * https://developer.mozilla.org/en-US/docs/Web/API/ElementInternals/validationMessage
     */
    readonly validationMessage: string;
    /**
     * Returns whether an element will successfully validate based on forms
     * validation rules and constraints.
     *
     * Disabled and readonly elements will not validate.
     *
     * https://developer.mozilla.org/en-US/docs/Web/API/ElementInternals/willValidate
     */
    readonly willValidate: boolean;
    /**
     * Checks the element's constraint validation and returns true if the element
     * is valid or false if not.
     *
     * If invalid, this method will dispatch an `invalid` event.
     *
     * https://developer.mozilla.org/en-US/docs/Web/API/ElementInternals/checkValidity
     *
     * @return true if the element is valid, or false if not.
     */
    checkValidity(): boolean;
    /**
     * Checks the element's constraint validation and returns true if the element
     * is valid or false if not.
     *
     * If invalid, this method will dispatch a cancelable `invalid` event. If not
     * canceled, a the current `validationMessage` will be reported to the user.
     *
     * https://developer.mozilla.org/en-US/docs/Web/API/ElementInternals/reportValidity
     *
     * @return true if the element is valid, or false if not.
     */
    reportValidity(): boolean;
    /**
     * Sets the element's constraint validation error message. When set to a
     * non-empty string, `validity.customError` will be true and
     * `validationMessage` will display the provided error.
     *
     * Use this method to customize error messages reported.
     *
     * https://developer.mozilla.org/en-US/docs/Web/API/HTMLInputElement/setCustomValidity
     *
     * @param error The error message to display, or an empty string.
     */
    setCustomValidity(error: string): void;
    /**
     * Creates and returns a `Validator` that is used to compute and cache
     * validity for the element.
     *
     * A validator that caches validity is important since constraint validation
     * must be computed synchronously and frequently in response to constraint
     * validation property changes.
     */
    [createValidator](): Validator<unknown>;
    /**
     * Returns shadow DOM child that is used as the anchor for the platform
     * `reportValidity()` popup. This is often the root element or the inner
     * focus-delegated element.
     */
    [getValidityAnchor](): HTMLElement | null;
}
/**
 * A symbol property used to create a constraint validation `Validator`.
 * Required for all `mixinConstraintValidation()` elements.
 */
export declare const createValidator: unique symbol;
/**
 * A symbol property used to return an anchor for constraint validation popups.
 * Required for all `mixinConstraintValidation()` elements.
 */
export declare const getValidityAnchor: unique symbol;
/**
 * Mixes in constraint validation APIs for an element.
 *
 * See https://developer.mozilla.org/en-US/docs/Web/HTML/Constraint_validation
 * for more details.
 *
 * Implementations must provide a validator to cache and compute its validity,
 * along with a shadow root element to anchor validation popups to.
 *
 * @example
 * ```ts
 * const baseClass = mixinConstraintValidation(
 *   mixinFormAssociated(mixinElementInternals(LitElement))
 * );
 *
 * class MyCheckbox extends baseClass {
 *   \@property({type: Boolean}) checked = false;
 *   \@property({type: Boolean}) required = false;
 *
 *   [createValidator]() {
 *     return new CheckboxValidator(() => this);
 *   }
 *
 *   [getValidityAnchor]() {
 *     return this.renderRoot.querySelector('.root');
 *   }
 * }
 * ```
 *
 * @param base The class to mix functionality into.
 * @return The provided class with `ConstraintValidation` mixed in.
 */
export declare function mixinConstraintValidation<T extends MixinBase<LitElement & FormAssociated & WithElementInternals>>(base: T): MixinReturn<T, ConstraintValidation>;
