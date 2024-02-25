/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Tabs } from './internal/tabs.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-tabs': MdTabs;
    }
}
/**
 * @summary Tabs displays a list of selectable tabs.
 *
 */
export declare class MdTabs extends Tabs {
    static styles: import("lit").CSSResult[];
}
