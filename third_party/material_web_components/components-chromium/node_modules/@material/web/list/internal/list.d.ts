/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement } from 'lit';
import { ListItem as SharedListItem } from './list-navigation-helpers.js';
interface ListItem extends SharedListItem {
    type: 'text' | 'button' | 'link';
}
export declare class List extends LitElement {
    /**
     * An array of activatable and disableable list items. Queries every assigned
     * element that has the `md-list-item` attribute.
     *
     * _NOTE:_ This is a shallow, flattened query via
     * `HTMLSlotElement.queryAssignedElements` and thus will _only_ include direct
     * children / directly slotted elements.
     */
    protected slotItems: Array<ListItem | (HTMLElement & {
        item?: ListItem;
    })>;
    /** @export */
    get items(): ListItem[];
    private readonly listController;
    private readonly internals;
    constructor();
    protected render(): import("lit-html").TemplateResult<1>;
    /**
     * Activates the next item in the list. If at the end of the list, the first
     * item will be activated.
     *
     * @return The activated list item or `null` if there are no items.
     */
    activateNextItem(): ListItem | null;
    /**
     * Activates the previous item in the list. If at the start of the list, the
     * last item will be activated.
     *
     * @return The activated list item or `null` if there are no items.
     */
    activatePreviousItem(): ListItem | null;
}
export {};
