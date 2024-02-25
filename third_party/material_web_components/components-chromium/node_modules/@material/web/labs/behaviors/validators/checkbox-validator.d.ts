/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Validator } from './validator.js';
/**
 * Constraint validation properties for a checkbox.
 */
export interface CheckboxState {
    /**
     * Whether the checkbox is checked.
     */
    readonly checked: boolean;
    /**
     * Whether the checkbox is required.
     */
    readonly required: boolean;
}
/**
 * A validator that provides constraint validation that emulates
 * `<input type="checkbox">` validation.
 */
export declare class CheckboxValidator extends Validator<CheckboxState> {
    private checkboxControl?;
    protected computeValidity(state: CheckboxState): {
        validity: ValidityState;
        validationMessage: string;
    };
    protected equals(prev: CheckboxState, next: CheckboxState): boolean;
    protected copy({ checked, required }: CheckboxState): {
        checked: boolean;
        required: boolean;
    };
}
