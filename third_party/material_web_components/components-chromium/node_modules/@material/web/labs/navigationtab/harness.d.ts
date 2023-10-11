/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Harness } from '../../testing/harness.js';
import { NavigationTab } from './internal/navigation-tab.js';
/**
 * Test harness for navigation tab elements.
 */
export declare class NavigationTabHarness extends Harness<NavigationTab> {
    getInteractiveElement(): Promise<HTMLElement>;
}
