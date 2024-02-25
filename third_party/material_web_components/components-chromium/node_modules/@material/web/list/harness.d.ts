/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Harness } from '../testing/harness.js';
import { List } from './internal/list.js';
import { ListItemHarness } from './internal/listitem/harness.js';
import { ListItemEl } from './internal/listitem/list-item.js';
export { ListItemHarness } from './internal/listitem/harness.js';
declare const NAVIGABLE_KEYS: {
    readonly ArrowDown: "ArrowDown";
    readonly ArrowUp: "ArrowUp";
    readonly Home: "Home";
    readonly End: "End";
};
/**
 * Keys that are handled by MdList. Keys not included in this are not handled by
 * MdList and should be dispatched by yourself.
 */
export type HandledListKeys = (typeof NAVIGABLE_KEYS)[keyof typeof NAVIGABLE_KEYS];
/**
 * Test harness for list.
 */
export declare class ListHarness extends Harness<List> {
    /**
     * Returns the first list item element.
     */
    protected getInteractiveElement(): Promise<List>;
    /** @return List item harnesses. */
    getItems(): ListItemHarness<ListItemEl>[];
    /**
     * Constructs keyboard events that are handled by List and makes sure that
     * they are constructed in a manner that List understands.
     *
     * @param key The key to dispatch on the list.
     */
    pressHandledKey<T extends string = HandledListKeys>(key: T): Promise<void>;
    /**
     * Dispatches a keypress on the list. It may or may not be a supported event.
     *
     * @param key The key to dispatch on the list.
     */
    keypress(key: string, init?: KeyboardEventInit): Promise<void>;
}
