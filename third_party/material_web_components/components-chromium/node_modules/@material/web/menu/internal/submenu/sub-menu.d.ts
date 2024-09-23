/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement } from 'lit';
import { MenuItem } from '../controllers/menuItemController.js';
import { Menu } from '../controllers/shared.js';
import { Corner } from '../menu.js';
/**
 * @fires deactivate-items {Event} Requests the parent menu to deselect other
 * items when a submenu opens. --bubbles --composed
 * @fires request-activation {Event} Requests the parent to make the slotted item
 * focusable and focus the item. --bubbles --composed
 * @fires deactivate-typeahead {Event} Requests the parent menu to deactivate
 * the typeahead functionality when a submenu opens. --bubbles --composed
 * @fires activate-typeahead {Event} Requests the parent menu to activate the
 * typeahead functionality when a submenu closes. --bubbles --composed
 */
export declare class SubMenu extends LitElement {
    /**
     * The anchorCorner to set on the submenu.
     */
    anchorCorner: Corner;
    /**
     * The menuCorner to set on the submenu.
     */
    menuCorner: Corner;
    /**
     * The delay between mouseenter and submenu opening.
     */
    hoverOpenDelay: number;
    /**
     * The delay between ponterleave and the submenu closing.
     */
    hoverCloseDelay: number;
    /**
     * READONLY: self-identifies as a menu item and sets its identifying attribute
     */
    isSubMenu: boolean;
    get item(): MenuItem;
    get menu(): Menu;
    private readonly items;
    private readonly menus;
    private previousOpenTimeout;
    private previousCloseTimeout;
    constructor();
    render(): import("lit-html").TemplateResult<1>;
    protected firstUpdated(): void;
    /**
     * Shows the submenu.
     */
    show(): Promise<void>;
    /**
     * Closes the submenu.
     */
    close(): Promise<void>;
    protected onSlotchange(): void;
    /**
     * Starts the default 400ms countdown to open the submenu.
     *
     * NOTE: We explicitly use mouse events and not pointer events because
     * pointer events apply to touch events. And if a user were to tap a
     * sub-menu, it would fire the "pointerenter", "pointerleave", "click" events
     * which would open the menu on click, and then set the timeout to close the
     * menu due to pointerleave.
     */
    protected onMouseenter: () => void;
    /**
     * Starts the default 400ms countdown to close the submenu.
     *
     * NOTE: We explicitly use mouse events and not pointer events because
     * pointer events apply to touch events. And if a user were to tap a
     * sub-menu, it would fire the "pointerenter", "pointerleave", "click" events
     * which would open the menu on click, and then set the timeout to close the
     * menu due to pointerleave.
     */
    protected onMouseleave: () => void;
    protected onClick(): void;
    /**
     * On item keydown handles opening the submenu.
     */
    protected onKeydown(event: KeyboardEvent): Promise<void>;
    private onCloseSubmenu;
    private onSubMenuKeydown;
    /**
     * Determines whether the given KeyboardEvent code is one that should open
     * the submenu. This is RTL-aware. By default, left, right, space, or enter.
     *
     * @param code The native KeyboardEvent code.
     * @return Whether or not the key code should open the submenu.
     */
    private isSubmenuOpenKey;
    /**
     * Determines whether the given KeyboardEvent code is one that should close
     * the submenu. This is RTL-aware. By default right, left, or escape.
     *
     * @param code The native KeyboardEvent code.
     * @return Whether or not the key code should close the submenu.
     */
    private isSubmenuCloseKey;
}
