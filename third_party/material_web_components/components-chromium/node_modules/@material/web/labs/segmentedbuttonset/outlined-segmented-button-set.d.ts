/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { CSSResultOrNative } from 'lit';
import { OutlinedSegmentedButtonSet } from './internal/outlined-segmented-button-set.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-outlined-segmented-button-set': MdOutlinedSegmentedButtonSet;
    }
}
/**
 * MdOutlinedSegmentedButtonSet is the custom element for the Material
 * Design outlined segmented button set component.
 * @final
 * @suppress {visibility}
 */
export declare class MdOutlinedSegmentedButtonSet extends OutlinedSegmentedButtonSet {
    static styles: CSSResultOrNative[];
}
