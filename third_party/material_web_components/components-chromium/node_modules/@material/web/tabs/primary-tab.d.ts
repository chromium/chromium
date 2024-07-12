/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { CSSResultOrNative } from 'lit';
import { PrimaryTab } from './internal/primary-tab.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-primary-tab': MdPrimaryTab;
    }
}
/**
 * @summary Tab allow users to display a tab within a Tabs.
 *
 * @final
 * @suppress {visibility}
 */
export declare class MdPrimaryTab extends PrimaryTab {
    static styles: CSSResultOrNative[];
}
