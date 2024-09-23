/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
export interface ListItem extends HTMLElement {
    disabled: boolean;
}
/**
 * A record that describes a list item in a list with metadata such a reference
 * to the item and its index in the list.
 */
export interface ItemRecord<Item extends ListItem> {
    item: Item;
    index: number;
}
/**
 * Activates the first non-disabled item of a given array of items.
 *
 * @param items {Array<ListItem>} The items from which to activate the
 *     first item.
 * @param isActivatable Function to determine if an item can be  activated.
 *     Defaults to non-disabled items.
 */
export declare function activateFirstItem<Item extends ListItem>(items: Item[], isActivatable?: (item: Item) => boolean): Item;
/**
 * Activates the last non-disabled item of a given array of items.
 *
 * @param items {Array<ListItem>} The items from which to activate the
 *     last item.
 * @param isActivatable Function to determine if an item can be  activated.
 *     Defaults to non-disabled items.
 * @nocollapse
 */
export declare function activateLastItem<Item extends ListItem>(items: Item[], isActivatable?: (item: Item) => boolean): Item;
/**
 * Deactivates the currently active item of a given array of items.
 *
 * @param items {Array<ListItem>} The items from which to deactivate the
 *     active item.
 * @param isActivatable Function to determine if an item can be  activated.
 *     Defaults to non-disabled items.
 * @return A record of the deleselcted activated item including the item and
 *     the index of the item or `null` if none are deactivated.
 * @nocollapse
 */
export declare function deactivateActiveItem<Item extends ListItem>(items: Item[], isActivatable?: (item: Item) => boolean): ItemRecord<Item>;
/**
 * Retrieves the first activated item of a given array of items.
 *
 * @param items {Array<ListItem>} The items to search.
 * @param isActivatable Function to determine if an item can be  activated.
 *     Defaults to non-disabled items.
 * @return A record of the first activated item including the item and the
 *     index of the item or `null` if none are activated.
 * @nocollapse
 */
export declare function getActiveItem<Item extends ListItem>(items: Item[], isActivatable?: (item: Item) => boolean): ItemRecord<Item>;
/**
 * Retrieves the first non-disabled item of a given array of items. This
 * the first item that is not disabled.
 *
 * @param items {Array<ListItem>} The items to search.
 * @param isActivatable Function to determine if an item can be  activated.
 *     Defaults to non-disabled items.
 * @return The first activatable item or `null` if none are activatable.
 * @nocollapse
 */
export declare function getFirstActivatableItem<Item extends ListItem>(items: Item[], isActivatable?: (item: Item) => boolean): Item;
/**
 * Retrieves the last non-disabled item of a given array of items.
 *
 * @param items {Array<ListItem>} The items to search.
 * @param isActivatable Function to determine if an item can be  activated.
 *     Defaults to non-disabled items.
 * @return The last activatable item or `null` if none are activatable.
 * @nocollapse
 */
export declare function getLastActivatableItem<Item extends ListItem>(items: Item[], isActivatable?: (item: Item) => boolean): Item;
/**
 * Retrieves the next non-disabled item of a given array of items.
 *
 * @param items {Array<ListItem>} The items to search.
 * @param index {{index: number}} The index to search from.
 * @param isActivatable Function to determine if an item can be  activated.
 *     Defaults to non-disabled items.
 * @param wrap If true, then the next item at the end of the list is the first
 *     item. Defaults to true.
 * @return The next activatable item or `null` if none are activatable.
 */
export declare function getNextItem<Item extends ListItem>(items: Item[], index: number, isActivatable?: (item: Item) => boolean, wrap?: boolean): Item;
/**
 * Retrieves the previous non-disabled item of a given array of items.
 *
 * @param items {Array<ListItem>} The items to search.
 * @param index {{index: number}} The index to search from.
 * @param isActivatable Function to determine if an item can be  activated.
 *     Defaults to non-disabled items.
 * @param wrap If true, then the previous item at the beginning of the list is
 *     the last item. Defaults to true.
 * @return The previous activatable item or `null` if none are activatable.
 */
export declare function getPrevItem<Item extends ListItem>(items: Item[], index: number, isActivatable?: (item: Item) => boolean, wrap?: boolean): Item;
/**
 * Activates the next item and focuses it. If nothing is currently activated,
 * activates the first item.
 */
export declare function activateNextItem<Item extends ListItem>(items: Item[], activeItemRecord: null | ItemRecord<Item>, isActivatable?: (item: Item) => boolean, wrap?: boolean): Item | null;
/**
 * Activates the previous item and focuses it. If nothing is currently
 * activated, activates the last item.
 */
export declare function activatePreviousItem<Item extends ListItem>(items: Item[], activeItemRecord: null | ItemRecord<Item>, isActivatable?: (item: Item) => boolean, wrap?: boolean): Item | null;
/**
 * Creates an event that requests the parent md-list to deactivate all other
 * items.
 */
export declare function createDeactivateItemsEvent(): Event;
/**
 * The type of the event that requests the parent md-list to deactivate all
 * other items.
 */
export type DeactivateItemsEvent = ReturnType<typeof createDeactivateItemsEvent>;
/**
 * Creates an event that requests the menu to set `tabindex=0` on the item and
 * focus it. We use this pattern because List keeps track of what element is
 * active in the List by maintaining tabindex. We do not want list items
 * to set tabindex on themselves or focus themselves so that we can organize all
 * that logic in the parent List and Menus, and list item stays as dumb as
 * possible.
 */
export declare function createRequestActivationEvent(): Event;
/**
 * The type of the event that requests the list activates and focuses the item.
 */
export type RequestActivationEvent = ReturnType<typeof createRequestActivationEvent>;
