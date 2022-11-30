/**
 * @license
 * Copyright 2018 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { ButtonBase } from './mwc-button-base.js';
/** @soyCompatible */
export declare class Button extends ButtonBase {
    static styles: import("lit").CSSResult[];
}
declare global {
    interface HTMLElementTagNameMap {
        'mwc-button': Button;
    }
}
