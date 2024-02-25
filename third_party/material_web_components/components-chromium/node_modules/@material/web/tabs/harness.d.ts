/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Harness } from '../testing/harness.js';
import { Tab } from './internal/tab.js';
import { Tabs } from './internal/tabs.js';
/**
 * Test harness for Tab.
 */
export declare class TabHarness extends Harness<Tab> {
    getInteractiveElement(): Promise<HTMLElement>;
    private completeIndicatorAnimation;
    isIndicatorShowing(): Promise<boolean>;
}
/**
 * Test harness for Tabs.
 */
export declare class TabsHarness extends Harness<Tabs> {
    getInteractiveElement(): Promise<HTMLElement>;
    get harnessedItems(): TabHarness[];
}
