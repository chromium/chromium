/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { FocusRing } from './lib/focus-ring.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-focus-ring': MdFocusRing;
    }
}
/**
 * @soyCompatible
 * @final
 * @suppress {visibility}
 */
export declare class MdFocusRing extends FocusRing {
    static styles: import("lit").CSSResult[];
}
