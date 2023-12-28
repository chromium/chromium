/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Validator } from './validator.js';
/**
 * Constraint validation properties for a radio.
 */
export interface RadioState {
    /**
     * Whether the radio is checked.
     */
    readonly checked: boolean;
    /**
     * Whether the radio is required.
     */
    readonly required: boolean;
}
/**
 * Radio constraint validation properties for a single radio and its siblings.
 */
export type RadioGroupState = readonly [RadioState, ...RadioState[]];
/**
 * A validator that provides constraint validation that emulates
 * `<input type="radio">` validation.
 */
export declare class RadioValidator extends Validator<RadioGroupState> {
    private radioElement?;
    protected computeValidity(states: RadioGroupState): {
        validity: {
            valueMissing: boolean;
        };
        validationMessage: string;
    };
    protected equals(prevGroup: RadioGroupState, nextGroup: RadioGroupState): boolean;
    protected copy(states: RadioGroupState): RadioGroupState;
}
