/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Icon } from './internal/icon.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-icon': MdIcon;
    }
}
/**
 * @final
 * @suppress {visibility}
 */
export declare class MdIcon extends Icon {
    /** @nocollapse */
    static styles: import("lit").CSSResult[];
}
