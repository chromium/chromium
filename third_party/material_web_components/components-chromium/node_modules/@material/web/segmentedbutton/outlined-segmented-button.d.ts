/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { OutlinedSegmentedButton } from './lib/outlined-segmented-button.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-outlined-segmented-button': MdOutlinedSegmentedButton;
    }
}
/**
 * MdOutlinedSegmentedButton is the custom element for the Material
 * Design outlined segmented button component.
 * @soyCompatible
 * @final
 * @suppress {visibility}
 */
export declare class MdOutlinedSegmentedButton extends OutlinedSegmentedButton {
    static styles: import("lit").CSSResult[];
}
