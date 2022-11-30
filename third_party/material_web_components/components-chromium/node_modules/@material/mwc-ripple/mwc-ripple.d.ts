/**
 * @license
 * Copyright 2018 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { RippleBase } from './mwc-ripple-base.js';
declare global {
    interface HTMLElementTagNameMap {
        'mwc-ripple': Ripple;
    }
}
/** @soyCompatible */
export declare class Ripple extends RippleBase {
    static styles: import("lit").CSSResult[];
}
