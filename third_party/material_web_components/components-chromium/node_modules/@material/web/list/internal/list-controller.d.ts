/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { ListItem } from './list-navigation-helpers.js';
/**
 * Default keys that trigger navigation.
 */
export declare const NavigableKeys: {
    readonly ArrowDown: "ArrowDown";
    readonly ArrowLeft: "ArrowLeft";
    readonly ArrowUp: "ArrowUp";
    readonly ArrowRight: "ArrowRight";
    readonly Home: "Home";
    readonly End: "End";
};
/**
 * Default set of navigable keys.
 */
export type NavigableKeys = (typeof NavigableKeys)[keyof typeof NavigableKeys];
/**
 * The configuration object to customize the behavior of the List Controller
 */
export interface ListControllerConfig<Item extends ListItem> {
    /**
     * A function that determines whether or not the given element is an Item
     */
    isItem: (item: HTMLElement) => item is Item;
    /**
     * A function that returns an array of elements to consider as items. For
     * example, all the slotted elements.
     */
    getPossibleItems: () => HTMLElement[];
    /**
     * A function that returns whether or not the list is in an RTL context.
     */
    isRtl: () => boolean;
    /**
     * Deactivates an item such as setting the tabindex to -1 and or sets selected
     * to false.
     */
    deactivateItem: (item: Item) => void;
    /**
     * Activates an item such as setting the tabindex to 1 and or sets selected to
     * true (but does not focus).
     */
    activateItem: (item: Item) => void;
    /**
     * Whether or not the key should be handled by the list for navigation.
     */
    isNavigableKey: (key: string) => boolean;
    /**
     * Whether or not the item can be activated. Defaults to items that are not
     * disabled.
     */
    isActivatable?: (item: Item) => boolean;
    /**
     * Whether or not navigating past the end of the list wraps to the beginning
     * and vice versa. Defaults to true.
     */
    wrapNavigation?: () => boolean;
}
/**
 * A controller that handles list keyboard navigation and item management.
 */
export declare class ListController<Item extends ListItem> {
    isItem: (item: HTMLElement) => item is Item;
    private readonly getPossibleItems;
    private readonly isRtl;
    private readonly deactivateItem;
    private readonly activateItem;
    private readonly isNavigableKey;
    private readonly isActivatable?;
    private readonly wrapNavigation;
    constructor(config: ListControllerConfig<Item>);
    /**
     * The items being managed by the list. Additionally, attempts to see if the
     * object has a sub-item in the `.item` property.
     */
    get items(): Item[];
    /**
     * Handles keyboard navigation. Should be bound to the node that will act as
     * the List.
     */
    handleKeydown: (event: KeyboardEvent) => void;
    /**
     * Activates the next item in the list. If at the end of the list, the first
     * item will be activated.
     *
     * @return The activated list item or `null` if there are no items.
     */
    activateNextItem(): Item | null;
    /**
     * Activates the previous item in the list. If at the start of the list, the
     * last item will be activated.
     *
     * @return The activated list item or `null` if there are no items.
     */
    activatePreviousItem(): Item | null;
    /**
     * Listener to be bound to the `deactivate-items` item event.
     */
    onDeactivateItems: () => void;
    /**
     * Listener to be bound to the `request-activation` item event..
     */
    onRequestActivation: (event: Event) => void;
    /**
     * Listener to be bound to the `slotchange` event for the slot that renders
     * the items.
     */
    onSlotchange: () => void;
}
