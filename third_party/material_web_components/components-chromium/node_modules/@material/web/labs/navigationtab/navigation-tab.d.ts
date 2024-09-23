/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { CSSResultOrNative } from 'lit';
import { NavigationTab } from './internal/navigation-tab.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-navigation-tab': MdNavigationTab;
    }
}
/**
 * @final
 * @suppress {visibility}
 */
export declare class MdNavigationTab extends NavigationTab {
    static styles: CSSResultOrNative[];
}
