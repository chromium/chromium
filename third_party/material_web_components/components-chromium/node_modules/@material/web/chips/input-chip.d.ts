/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { CSSResultOrNative } from 'lit';
import { InputChip } from './internal/input-chip.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-input-chip': MdInputChip;
    }
}
/**
 * TODO(b/243982145): add docs
 *
 * @final
 * @suppress {visibility}
 */
export declare class MdInputChip extends InputChip {
    static styles: CSSResultOrNative[];
}
