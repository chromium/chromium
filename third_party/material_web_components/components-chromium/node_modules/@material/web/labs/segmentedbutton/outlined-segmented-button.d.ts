/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { CSSResultOrNative } from 'lit';
import { OutlinedSegmentedButton } from './internal/outlined-segmented-button.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-outlined-segmented-button': MdOutlinedSegmentedButton;
    }
}
/**
 * MdOutlinedSegmentedButton is the custom element for the Material
 * Design outlined segmented button component.
 * @final
 * @suppress {visibility}
 */
export declare class MdOutlinedSegmentedButton extends OutlinedSegmentedButton {
    static styles: CSSResultOrNative[];
}
