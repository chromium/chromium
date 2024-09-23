/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { CSSResultOrNative } from 'lit';
import { FilledField } from './internal/filled-field.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-filled-field': MdFilledField;
    }
}
/**
 * TODO(b/228525797): add docs
 * @final
 * @suppress {visibility}
 */
export declare class MdFilledField extends FilledField {
    static styles: CSSResultOrNative[];
}
