/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement } from 'lit';
import { MenuItem } from './menuItemController.js';
import type { Corner, SurfacePositionTarget } from './surfacePositionController.js';
/**
 * The interface needed for a Menu to work with other md-menu elements.
 */
export interface MenuSelf {
    /**
     * Whether or not the menu is currently opened.
     */
    open: boolean;
    /**
     * Skips the opening and closing animations.
     */
    quick: boolean;
    /**
     * Displays overflow content like a submenu.
     *
     * __NOTE__: This may cause adverse effects if you set
     * `md-menu {max-height:...}`
     * and have items overflowing items in the "y" direction.
     */
    hasOverflow: boolean;
    /**
     * Communicates to the menu that it is a submenu and should not handle the
     * ArrowLeft button in LTR and ArrowRight button in RTL.
     */
    isSubmenu: boolean;
    /**
     * After closing, does not restore focus to the last focused element before
     * the menu was opened.
     */
    skipRestoreFocus: boolean;
    /**
     * The corner of the anchor in which the menu should anchor to.
     */
    anchorCorner: Corner;
    /**
     * The corner of the menu in which the menu should anchor from.
     */
    menuCorner: Corner;
    /**
     * The element the menu should anchor to.
     */
    anchorElement: (HTMLElement & Partial<SurfacePositionTarget>) | null;
    /**
     * What the menu should focus by default when opened.
     */
    defaultFocus: FocusState;
    /**
     * An array of items managed by the list.
     */
    items: MenuItem[];
    /**
     * The positioning strategy of the menu.
     *
     * - `absolute` is relative to the anchor element.
     * - `fixed` is relative to the window
     * - `document` is relative to the document
     */
    positioning?: 'absolute' | 'fixed' | 'document';
    /**
     * Opens the menu.
     */
    show: () => void;
    /**
     * Closes the menu.
     */
    close: () => void;
}
/**
 * The interface needed for a Menu to work with other md-menu elements. Useful
 * for keeping your types safe when wrapping `md-menu`.
 */
export type Menu = MenuSelf & LitElement;
/**
 * The reason the `close-menu` event was dispatched.
 */
export interface Reason {
    kind: string;
}
/**
 * The click selection reason for the `close-menu` event. The menu was closed
 * because an item was selected via user click.
 */
export interface ClickReason extends Reason {
    kind: typeof CloseReason.CLICK_SELECTION;
}
/**
 * The keydown reason for the `close-menu` event. The menu was closed
 * because a specific key was pressed. The default closing keys for
 * `md-menu-item` are, Space, Enter or Escape.
 */
export interface KeydownReason extends Reason {
    kind: typeof CloseReason.KEYDOWN;
    key: string;
}
/**
 * The default menu closing reasons for the material md-menu package.
 */
export type DefaultReasons = ClickReason | KeydownReason;
/**
 * Creates an event that closes any parent menus.
 */
export declare function createCloseMenuEvent<T extends Reason = DefaultReasons>(initiator: MenuItem, reason: T): CustomEvent<{
    initiator: MenuItem;
    itemPath: MenuItem[];
    reason: T;
}>;
/**
 * Creates an event that signals to the menu that it should stay open on the
 * focusout event.
 */
export declare function createStayOpenOnFocusoutEvent(): Event;
/**
 * Creates an event that signals to the menu that it should close open on the
 * focusout event.
 */
export declare function createCloseOnFocusoutEvent(): Event;
/**
 * Creates a default close menu event used by md-menu.
 */
export declare const createDefaultCloseMenuEvent: (initiator: MenuItem, reason: DefaultReasons) => CustomEvent<{
    initiator: MenuItem;
    itemPath: MenuItem[];
    reason: DefaultReasons;
}>;
/**
 * The type of the default close menu event used by md-menu.
 */
export type CloseMenuEvent<T extends Reason = DefaultReasons> = ReturnType<typeof createCloseMenuEvent<T>>;
/**
 * Creates an event that requests the given item be selected.
 */
export declare function createDeactivateTypeaheadEvent(): Event;
/**
 * The type of the event that requests the typeahead functionality of containing
 * menu be deactivated.
 */
export type DeactivateTypeaheadEvent = ReturnType<typeof createDeactivateTypeaheadEvent>;
/**
 * Creates an event that requests the typeahead functionality of containing menu
 * be activated.
 */
export declare function createActivateTypeaheadEvent(): Event;
/**
 * The type of the event that requests the typeahead functionality of containing
 * menu be activated.
 */
export type ActivateTypeaheadEvent = ReturnType<typeof createActivateTypeaheadEvent>;
/**
 * Keys that are used to navigate menus.
 */
export declare const NavigableKey: {
    readonly UP: "ArrowUp";
    readonly DOWN: "ArrowDown";
    readonly RIGHT: "ArrowRight";
    readonly LEFT: "ArrowLeft";
};
/**
 * Keys that are used for selection in menus.
 */
export declare const SelectionKey: {
    readonly SPACE: "Space";
    readonly ENTER: "Enter";
};
/**
 * Default close `Reason` kind values.
 */
export declare const CloseReason: {
    readonly CLICK_SELECTION: "click-selection";
    readonly KEYDOWN: "keydown";
};
/**
 * Keys that can close menus.
 */
export declare const KeydownCloseKey: {
    readonly ESCAPE: "Escape";
    readonly SPACE: "Space";
    readonly ENTER: "Enter";
};
type Values<T> = T[keyof T];
/**
 * Determines whether the given key code is a key code that should close the
 * menu.
 *
 * @param code The KeyboardEvent code to check.
 * @return Whether or not the key code is in the predetermined list to close the
 * menu.
 */
export declare function isClosableKey(code: string): code is Values<typeof KeydownCloseKey>;
/**
 * Determines whether the given key code is a key code that should select a menu
 * item.
 *
 * @param code They KeyboardEvent code to check.
 * @return Whether or not the key code is in the predetermined list to select a
 * menu item.
 */
export declare function isSelectableKey(code: string): code is Values<typeof SelectionKey>;
/**
 * Determines whether a target element is contained inside another element's
 * composed tree.
 *
 * @param target The potential contained element.
 * @param container The potential containing element of the target.
 * @returns Whether the target element is contained inside the container's
 * composed subtree
 */
export declare function isElementInSubtree(target: EventTarget, container: EventTarget): boolean;
/**
 * Element to focus on when menu is first opened.
 */
export declare const FocusState: {
    readonly NONE: "none";
    readonly LIST_ROOT: "list-root";
    readonly FIRST_ITEM: "first-item";
    readonly LAST_ITEM: "last-item";
};
/**
 * Element to focus on when menu is first opened.
 */
export type FocusState = (typeof FocusState)[keyof typeof FocusState];
export {};
