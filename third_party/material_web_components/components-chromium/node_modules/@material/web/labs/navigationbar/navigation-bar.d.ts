/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { NavigationBar } from './internal/navigation-bar.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-navigation-bar': MdNavigationBar;
    }
}
/**
 * @final
 * @suppress {visibility}
 */
export declare class MdNavigationBar extends NavigationBar {
    static styles: import("lit").CSSResult[];
}
