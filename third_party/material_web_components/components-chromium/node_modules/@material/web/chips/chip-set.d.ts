/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { ChipSet } from './internal/chip-set.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-chip-set': MdChipSet;
    }
}
/**
 * TODO(b/243982145): add docs
 *
 * @final
 * @suppress {visibility}
 */
export declare class MdChipSet extends ChipSet {
    static styles: import("lit").CSSResult[];
}
