/**
 * @license
 * Copyright 2018 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { CSSResultOrNative } from 'lit';
import { Checkbox } from './internal/checkbox.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-checkbox': MdCheckbox;
    }
}
/**
 * @summary Checkboxes allow users to select one or more items from a set.
 * Checkboxes can turn an option on or off.
 *
 * @description
 * Use checkboxes to:
 * - Select one or more options from a list
 * - Present a list containing sub-selections
 * - Turn an item on or off in a desktop environment
 *
 * @final
 * @suppress {visibility}
 */
export declare class MdCheckbox extends Checkbox {
    static styles: CSSResultOrNative[];
}
