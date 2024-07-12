/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../field/outlined-field.js';
import { CSSResultOrNative } from 'lit';
import { OutlinedTextField } from './internal/outlined-text-field.js';
export { type TextFieldType } from './internal/text-field.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-outlined-text-field': MdOutlinedTextField;
    }
}
/**
 * TODO(b/228525797): Add docs
 * @final
 * @suppress {visibility}
 */
export declare class MdOutlinedTextField extends OutlinedTextField {
    static styles: CSSResultOrNative[];
    protected readonly fieldTag: import("lit-html/static.js").StaticValue;
}
