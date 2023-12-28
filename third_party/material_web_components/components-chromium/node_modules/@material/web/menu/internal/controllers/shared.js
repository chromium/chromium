/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * Creates an event that closes any parent menus.
 */
export function createCloseMenuEvent(initiator, reason) {
    return new CustomEvent('close-menu', {
        bubbles: true,
        composed: true,
        detail: { initiator, reason, itemPath: [initiator] },
    });
}
/**
 * Creates an event that signals to the menu that it should stay open on the
 * focusout event.
 */
export function createStayOpenOnFocusoutEvent() {
    return new Event('stay-open-on-focusout', { bubbles: true, composed: true });
}
/**
 * Creates an event that signals to the menu that it should close open on the
 * focusout event.
 */
export function createCloseOnFocusoutEvent() {
    return new Event('close-on-focusout', { bubbles: true, composed: true });
}
/**
 * Creates a default close menu event used by md-menu.
 */
export const createDefaultCloseMenuEvent = (createCloseMenuEvent);
/**
 * Creates an event that requests the given item be selected.
 */
export function createDeactivateTypeaheadEvent() {
    return new Event('deactivate-typeahead', { bubbles: true, composed: true });
}
/**
 * Creates an event that requests the typeahead functionality of containing menu
 * be activated.
 */
export function createActivateTypeaheadEvent() {
    return new Event('activate-typeahead', { bubbles: true, composed: true });
}
/**
 * Keys that are used to navigate menus.
 */
// tslint:disable-next-line:enforce-name-casing We are mimicking enum style
export const NavigableKey = {
    UP: 'ArrowUp',
    DOWN: 'ArrowDown',
    RIGHT: 'ArrowRight',
    LEFT: 'ArrowLeft',
};
/**
 * Keys that are used for selection in menus.
 */
// tslint:disable-next-line:enforce-name-casing We are mimicking enum style
export const SelectionKey = {
    SPACE: 'Space',
    ENTER: 'Enter',
};
/**
 * Default close `Reason` kind values.
 */
// tslint:disable-next-line:enforce-name-casing We are mimicking enum style
export const CloseReason = {
    CLICK_SELECTION: 'click-selection',
    KEYDOWN: 'keydown',
};
/**
 * Keys that can close menus.
 */
// tslint:disable-next-line:enforce-name-casing We are mimicking enum style
export const KeydownCloseKey = {
    ESCAPE: 'Escape',
    SPACE: SelectionKey.SPACE,
    ENTER: SelectionKey.ENTER,
};
/**
 * Determines whether the given key code is a key code that should close the
 * menu.
 *
 * @param code The KeyboardEvent code to check.
 * @return Whether or not the key code is in the predetermined list to close the
 * menu.
 */
export function isClosableKey(code) {
    return Object.values(KeydownCloseKey).some((value) => value === code);
}
/**
 * Determines whether the given key code is a key code that should select a menu
 * item.
 *
 * @param code They KeyboardEvent code to check.
 * @return Whether or not the key code is in the predetermined list to select a
 * menu item.
 */
export function isSelectableKey(code) {
    return Object.values(SelectionKey).some((value) => value === code);
}
/**
 * Determines whether a target element is contained inside another element's
 * composed tree.
 *
 * @param target The potential contained element.
 * @param container The potential containing element of the target.
 * @returns Whether the target element is contained inside the container's
 * composed subtree
 */
export function isElementInSubtree(target, container) {
    // Dispatch a composed, bubbling event to check its path to see if the
    // newly-focused element is contained in container's subtree
    const focusEv = new Event('md-contains', { bubbles: true, composed: true });
    let composedPath = [];
    const listener = (ev) => {
        composedPath = ev.composedPath();
    };
    container.addEventListener('md-contains', listener);
    target.dispatchEvent(focusEv);
    container.removeEventListener('md-contains', listener);
    const isContained = composedPath.length > 0;
    return isContained;
}
/**
 * Element to focus on when menu is first opened.
 */
// tslint:disable-next-line:enforce-name-casing We are mimicking enum style
export const FocusState = {
    NONE: 'none',
    LIST_ROOT: 'list-root',
    FIRST_ITEM: 'first-item',
    LAST_ITEM: 'last-item',
};
//# sourceMappingURL=shared.js.map