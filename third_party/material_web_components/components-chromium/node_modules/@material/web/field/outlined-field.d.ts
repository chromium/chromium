/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { OutlinedField } from './lib/outlined-field.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-outlined-field': MdOutlinedField;
    }
}
/**
 * @soyCompatible
 * @final
 * @suppress {visibility}
 */
export declare class MdOutlinedField extends OutlinedField {
    static styles: import("lit").CSSResult[];
}
