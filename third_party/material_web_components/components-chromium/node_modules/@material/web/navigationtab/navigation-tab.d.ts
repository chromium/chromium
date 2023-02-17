/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { NavigationTab } from './lib/navigation-tab.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-navigation-tab': MdNavigationTab;
    }
}
/**
 * @soyCompatible
 * @final
 * @suppress {visibility}
 */
export declare class MdNavigationTab extends NavigationTab {
    static styles: import("lit").CSSResult[];
}
