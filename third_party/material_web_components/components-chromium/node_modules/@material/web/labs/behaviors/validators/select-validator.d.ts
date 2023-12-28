/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Validator } from './validator.js';
/**
 * Constraint validation properties for a select dropdown.
 */
export interface SelectState {
    /**
     * The current selected value.
     */
    readonly value: string;
    /**
     * Whether the select is required.
     */
    readonly required: boolean;
}
/**
 * A validator that provides constraint validation that emulates `<select>`
 * validation.
 */
export declare class SelectValidator extends Validator<SelectState> {
    private selectControl?;
    protected computeValidity(state: SelectState): {
        validity: ValidityState;
        validationMessage: string;
    };
    protected equals(prev: SelectState, next: SelectState): boolean;
    protected copy({ value, required }: SelectState): {
        value: string;
        required: boolean;
    };
}
