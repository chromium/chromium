/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { OutlinedSegmentedButtonSet } from './lib/outlined-segmented-button-set.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-outlined-segmented-button-set': MdOutlinedSegmentedButtonSet;
    }
}
/**
 * MdOutlinedSegmentedButtonSet is the custom element for the Material
 * Design outlined segmented button set component.
 * @soyCompatible
 * @final
 * @suppress {visibility}
 */
export declare class MdOutlinedSegmentedButtonSet extends OutlinedSegmentedButtonSet {
    static styles: import("lit").CSSResult[];
}
