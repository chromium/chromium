/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement, TemplateResult } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
import { ARIARole } from '../../types/aria.js';
import { ListItem } from './listitem/list-item.js';
export declare class List extends LitElement {
    static shadowRootOptions: ShadowRootInit;
    ariaLabel: string;
    ariaActivedescendant: string;
    role: ARIARole;
    /**
     * The tabindex of the underlying list.
     */
    listTabIndex: number;
    listRoot: HTMLElement;
    /**
     * An array of activatable and disableable list items. Queries every assigned
     * element that has the `md-list-item` attribute.
     *
     * _NOTE:_ This is a shallow, flattened query via
     * `HTMLSlotElement.queryAssignedElements` and thus will _only_ include direct
     * children / directly slotted elements.
     */
    items: ListItem[];
    render(): TemplateResult;
    /**
     * Renders the main list element.
     */
    protected renderList(): TemplateResult<1>;
    /**
     * The classes to be applied to the underlying list.
     */
    protected getListClasses(): ClassInfo;
    /**
     * The content to be slotted into the list.
     */
    protected renderContent(): TemplateResult<1>;
    /**
     * Handles keyboard navigation in the list.
     *
     * @param event {KeyboardEvent} The keyboard event that triggers this handler.
     */
    handleKeydown(event: KeyboardEvent): void;
    /**
     * Activates the first non-disabled item of a given array of items.
     *
     * @param items {Array<ListItem>} The items from which to activate the
     * first item.
     */
    static activateFirstItem<T extends ListItem>(items: T[]): void;
    /**
     * Activates the last non-disabled item of a given array of items.
     *
     * @param items {Array<ListItem>} The items from which to activate the
     * last item.
     */
    static activateLastItem<T extends ListItem>(items: T[]): void;
    /**
     * Deactivates the currently active item of a given array of items.
     *
     * @param items {Array<ListItem>} The items from which to deactivate the
     * active item.
     * @returns A record of the deleselcted activated item including the item and
     * the index of the item or `null` if none are deactivated.
     */
    static deactivateActiveItem<T extends ListItem>(items: T[]): {
        item: T;
        index: number;
    };
    focus(): void;
    /**
     * Retrieves the the first activated item of a given array of items.
     *
     * @param items {Array<ListItem>} The items to search.
     * @returns A record of the first activated item including the item and the
     * index of the item or `null` if none are activated.
     */
    static getActiveItem<T extends ListItem>(items: T[]): {
        item: T;
        index: number;
    };
    /**
     * Retrieves the the first non-disabled item of a given array of items. This
     * the first item that is not disabled.
     *
     * @param items {Array<ListItem>} The items to search.
     * @returns The first activatable item or `null` if none are activatable.
     */
    static getFirstActivatableItem<T extends ListItem>(items: T[]): T;
    /**
     * Retrieves the the last non-disabled item of a given array of items.
     *
     * @param items {Array<ListItem>} The items to search.
     * @returns The last activatable item or `null` if none are activatable.
     */
    static getLastActivatableItem<T extends ListItem>(items: T[]): T;
    /**
     * Retrieves the the next non-disabled item of a given array of items.
     *
     * @param items {Array<ListItem>} The items to search.
     * @param index {{index: number}} The index to search from.
     * @returns The next activatable item or `null` if none are activatable.
     */
    protected static getNextItem<T extends ListItem>(items: T[], index: number): T;
    /**
     * Retrieves the the previous non-disabled item of a given array of items.
     *
     * @param items {Array<ListItem>} The items to search.
     * @param index {{index: number}} The index to search from.
     * @returns The previous activatable item or `null` if none are activatable.
     */
    protected static getPrevItem<T extends ListItem>(items: T[], index: number): T;
}
