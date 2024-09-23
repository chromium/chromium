/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { CSSResultOrNative } from 'lit';
import { SecondaryTab } from './internal/secondary-tab.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-secondary-tab': MdSecondaryTab;
    }
}
/**
 * @summary Tab allow users to display a tab within a Tabs.
 *
 * @final
 * @suppress {visibility}
 */
export declare class MdSecondaryTab extends SecondaryTab {
    static styles: CSSResultOrNative[];
}
