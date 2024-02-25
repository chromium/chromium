/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement } from 'lit';
import { Harness } from '../../../testing/harness.js';
import { NavigableKeys } from '../list-controller.js';
import { ListItem } from '../list-navigation-helpers.js';
/**
 * Keys that are handled by MdList. Keys not included in this are not handled by
 * MdList and should be dispatched by yourself.
 */
export type HandledListKeys = (typeof NavigableKeys)[keyof typeof NavigableKeys];
/**
 * Test harness for list item.
 */
export declare class ListItemHarness<T extends LitElement = ListItem & LitElement> extends Harness<T> {
    getInteractiveElement(): Promise<HTMLElement>;
    /**
     * Constructs keyboard events that are handled by List and makes sure that
     * they are constructed in a manner that List understands.
     *
     * @param key The key to dispatch on the list.
     */
    pressHandledKey<U extends string = HandledListKeys>(key: U): Promise<void>;
}
