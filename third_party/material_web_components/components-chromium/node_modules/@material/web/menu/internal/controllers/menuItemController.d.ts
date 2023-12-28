/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { ReactiveController, ReactiveControllerHost } from 'lit';
/**
 * Interface specific to menu item and not HTMLElement.
 *
 * NOTE: required properties are expected to be reactive.
 */
interface MenuItemAdditions {
    /**
     * Whether or not the item is in the disabled state.
     */
    disabled: boolean;
    /**
     * The text of the item that will be used for typeahead. If not set, defaults
     * to the textContent of the element slotted into the headline.
     */
    typeaheadText: string;
    /**
     * Whether or not the item is in the selected visual state.
     */
    selected: boolean;
    /**
     * Sets the behavior and role of the menu item, defaults to "menuitem".
     */
    type: MenuItemType;
    /**
     * Whether it should keep the menu open after click.
     */
    keepOpen?: boolean;
    /**
     * Sets the underlying `HTMLAnchorElement`'s `href` resource attribute.
     */
    href?: string;
    /**
     * Focuses the item.
     */
    focus: () => void;
}
/**
 * The interface of every menu item interactive with a menu. All menu items
 * should implement this interface to be compatible with md-menu. Additionally
 * it should have the `md-menu-item` attribute set.
 *
 * NOTE, the required properties are recommended to be reactive properties.
 */
export type MenuItem = MenuItemAdditions & HTMLElement;
/**
 * Supported behaviors for a menu item.
 */
export type MenuItemType = 'menuitem' | 'option' | 'button' | 'link';
/**
 * The options used to inialize MenuItemController.
 */
export interface MenuItemControllerConfig {
    /**
     * A function that returns the headline element of the menu item.
     */
    getHeadlineElements: () => HTMLElement[];
    /**
     * A function that returns the supporting-text element of the menu item.
     */
    getSupportingTextElements: () => HTMLElement[];
    /**
     * A function that returns the default slot / misc content.
     */
    getDefaultElements: () => Node[];
    /**
     * The HTML Element that accepts user interactions like click. Used for
     * occasions like programmatically clicking anchor tags when `Enter` is
     * pressed.
     */
    getInteractiveElement: () => HTMLElement | null;
}
/**
 * A controller that provides most functionality of an element that implements
 * the MenuItem interface.
 */
export declare class MenuItemController implements ReactiveController {
    private readonly host;
    private internalTypeaheadText;
    private readonly getHeadlineElements;
    private readonly getSupportingTextElements;
    private readonly getDefaultElements;
    private readonly getInteractiveElement;
    /**
     * @param host The MenuItem in which to attach this controller to.
     * @param config The object that configures this controller's behavior.
     */
    constructor(host: ReactiveControllerHost & MenuItem, config: MenuItemControllerConfig);
    /**
     * The text that is selectable via typeahead. If not set, defaults to the
     * innerText of the item slotted into the `"headline"` slot, and if there are
     * no slotted elements into headline, then it checks the _default_ slot, and
     * then the `"supporting-text"` slot if nothing is in _default_.
     */
    get typeaheadText(): string;
    /**
     * The recommended tag name to render as the list item.
     */
    get tagName(): "button" | "a" | "li";
    /**
     * The recommended role of the menu item.
     */
    get role(): "menuitem" | "option";
    hostConnected(): void;
    hostUpdate(): void;
    /**
     * Bind this click listener to the interactive element. Handles closing the
     * menu.
     */
    onClick: () => void;
    /**
     * Bind this click listener to the interactive element. Handles closing the
     * menu.
     */
    onKeydown: (event: KeyboardEvent) => void;
    /**
     * Use to set the typeaheadText when it changes.
     */
    setTypeaheadText(text: string): void;
}
export {};
