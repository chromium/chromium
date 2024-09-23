/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Field } from '../field/internal/field.js';
import { Harness } from '../testing/harness.js';
import { Select } from './internal/select.js';
import { SelectOptionHarness } from './internal/selectoption/harness.js';
/**
 * Test harness for menu.
 */
export declare class SelectHarness extends Harness<Select> {
    protected getField(): Field;
    /**
     * Shows the menu and returns the first list item element.
     */
    protected getInteractiveElement(): Promise<Field>;
    startHover(): Promise<void>;
    /** @return ListItem harnesses for the menu's items. */
    getItems(): SelectOptionHarness[];
    click(): Promise<void>;
    clickOption(index: number): Promise<void>;
    get isOpen(): boolean;
}
