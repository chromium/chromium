/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { ReactiveElement } from 'lit';
import { WithElementInternals } from '../../labs/behaviors/element-internals.js';
/**
 * A string indicating the form submission behavior of the element.
 *
 * - submit: The element submits the form. This is the default value if the
 * attribute is not specified, or if it is dynamically changed to an empty or
 * invalid value.
 * - reset: The element resets the form.
 * - button: The element does nothing.
 */
export type FormSubmitterType = 'button' | 'submit' | 'reset';
/**
 * An element that can submit or reset a `<form>`, similar to
 * `<button type="submit">`.
 */
export interface FormSubmitter extends ReactiveElement, WithElementInternals {
    /**
     * A string indicating the form submission behavior of the element.
     *
     * - submit: The element submits the form. This is the default value if the
     * attribute is not specified, or if it is dynamically changed to an empty or
     * invalid value.
     * - reset: The element resets the form.
     * - button: The element does nothing.
     */
    type: FormSubmitterType;
    /**
     * The HTML name to use in form submission. When combined with a `value`, the
     * submitting button's name/value will be added to the form.
     *
     * Names must reflect to a `name` attribute for form integration.
     */
    name: string;
    /**
     * The value of the button. When combined with a `name`, the submitting
     * button's name/value will be added to the form.
     */
    value: string;
}
type FormSubmitterConstructor = (new () => FormSubmitter) | (abstract new () => FormSubmitter);
/**
 * Sets up an element's constructor to enable form submission. The element
 * instance should be form associated and have a `type` property.
 *
 * A click listener is added to each element instance. If the click is not
 * default prevented, it will submit the element's form, if any.
 *
 * @example
 * ```ts
 * class MyElement extends mixinElementInternals(LitElement) {
 *   static {
 *     setupFormSubmitter(MyElement);
 *   }
 *
 *   static formAssociated = true;
 *
 *   type: FormSubmitterType = 'submit';
 * }
 * ```
 *
 * @param ctor The form submitter element's constructor.
 */
export declare function setupFormSubmitter(ctor: FormSubmitterConstructor): void;
export {};
