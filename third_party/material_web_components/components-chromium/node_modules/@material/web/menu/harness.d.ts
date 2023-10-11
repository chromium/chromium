/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Harness } from '../testing/harness.js';
import { Menu } from './internal/menu.js';
import { MenuItemHarness } from './internal/menuitem/harness.js';
export { MenuItemHarness } from './internal/menuitem/harness.js';
/**
 * Test harness for menu.
 */
export declare class MenuHarness extends Harness<Menu> {
    /**
     * Shows the menu and returns the first list item element.
     */
    protected getInteractiveElement(): Promise<Menu>;
    /** @return ListItem harnesses for the menu's items. */
    getItems(): MenuItemHarness[];
    show(): Promise<void>;
}
