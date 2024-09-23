/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../field/filled-field.js';
import { CSSResultOrNative } from 'lit';
import { FilledTextField } from './internal/filled-text-field.js';
export { type TextFieldType } from './internal/text-field.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-filled-text-field': MdFilledTextField;
    }
}
/**
 * TODO(b/228525797): Add docs
 * @final
 * @suppress {visibility}
 */
export declare class MdFilledTextField extends FilledTextField {
    static styles: CSSResultOrNative[];
    protected readonly fieldTag: import("lit-html/static.js").StaticValue;
}
