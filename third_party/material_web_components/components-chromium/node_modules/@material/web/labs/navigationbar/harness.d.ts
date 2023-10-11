/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Harness } from '../../testing/harness.js';
import { NavigationTabHarness } from '../navigationtab/harness.js';
import { NavigationBar } from './internal/navigation-bar.js';
/**
 * Test harness for navigation bars.
 */
export declare class NavigationBarHarness extends Harness<NavigationBar> {
    readonly tab: Promise<NavigationTabHarness>;
    /**
     * Returns the active tab to be used for interaction simulation.
     */
    protected getInteractiveElement(): Promise<HTMLElement>;
    protected getTab(): Promise<NavigationTabHarness>;
}
