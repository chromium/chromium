/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Ripple } from './lib/ripple.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-ripple': MdRipple;
    }
}
/**
 * @soyCompatible
 * @final
 * @suppress {visibility}
 */
export declare class MdRipple extends Ripple {
    static styles: import("lit").CSSResult[];
}
