/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { html, isServer, LitElement } from 'lit';
import { property, queryAssignedElements } from 'lit/decorators.js';
import { createDeactivateItemsEvent, createRequestActivationEvent, deactivateActiveItem, getFirstActivatableItem, } from '../../../list/internal/list-navigation-helpers.js';
import { CloseReason, createActivateTypeaheadEvent, createDeactivateTypeaheadEvent, KeydownCloseKey, NavigableKey, SelectionKey, } from '../controllers/shared.js';
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
export class SubMenu extends LitElement {
    get item() {
        return this.items[0] ?? null;
    }
    get menu() {
        return this.menus[0] ?? null;
    }
    constructor() {
        super();
        /**
         * The anchorCorner to set on the submenu.
         */
        this.anchorCorner = Corner.START_END;
        /**
         * The menuCorner to set on the submenu.
         */
        this.menuCorner = Corner.START_START;
        /**
         * The delay between mouseenter and submenu opening.
         */
        this.hoverOpenDelay = 400;
        /**
         * The delay between ponterleave and the submenu closing.
         */
        this.hoverCloseDelay = 400;
        /**
         * READONLY: self-identifies as a menu item and sets its identifying attribute
         */
        this.isSubMenu = true;
        this.previousOpenTimeout = 0;
        this.previousCloseTimeout = 0;
        /**
         * Starts the default 400ms countdown to open the submenu.
         *
         * NOTE: We explicitly use mouse events and not pointer events because
         * pointer events apply to touch events. And if a user were to tap a
         * sub-menu, it would fire the "pointerenter", "pointerleave", "click" events
         * which would open the menu on click, and then set the timeout to close the
         * menu due to pointerleave.
         */
        this.onMouseenter = () => {
            clearTimeout(this.previousOpenTimeout);
            clearTimeout(this.previousCloseTimeout);
            if (this.menu?.open)
                return;
            // Open synchronously if delay is 0. (screenshot tests infra
            // would never resolve otherwise)
            if (!this.hoverOpenDelay) {
                this.show();
            }
            else {
                this.previousOpenTimeout = setTimeout(() => {
                    this.show();
                }, this.hoverOpenDelay);
            }
        };
        /**
         * Starts the default 400ms countdown to close the submenu.
         *
         * NOTE: We explicitly use mouse events and not pointer events because
         * pointer events apply to touch events. And if a user were to tap a
         * sub-menu, it would fire the "pointerenter", "pointerleave", "click" events
         * which would open the menu on click, and then set the timeout to close the
         * menu due to pointerleave.
         */
        this.onMouseleave = () => {
            clearTimeout(this.previousCloseTimeout);
            clearTimeout(this.previousOpenTimeout);
            // Close synchronously if delay is 0. (screenshot tests infra
            // would never resolve otherwise)
            if (!this.hoverCloseDelay) {
                this.close();
            }
            else {
                this.previousCloseTimeout = setTimeout(() => {
                    this.close();
                }, this.hoverCloseDelay);
            }
        };
        if (!isServer) {
            this.addEventListener('mouseenter', this.onMouseenter);
            this.addEventListener('mouseleave', this.onMouseleave);
        }
    }
    render() {
        return html `
      <slot
        name="item"
        @click=${this.onClick}
        @keydown=${this.onKeydown}
        @slotchange=${this.onSlotchange}>
      </slot>
      <slot
        name="menu"
        @keydown=${this.onSubMenuKeydown}
        @close-menu=${this.onCloseSubmenu}
        @slotchange=${this.onSlotchange}>
      </slot>
    `;
    }
    firstUpdated() {
        // slotchange is not fired if the contents have been SSRd
        this.onSlotchange();
    }
    /**
     * Shows the submenu.
     */
    async show() {
        const menu = this.menu;
        if (!menu || menu.open)
            return;
        // Ensures that we deselect items when the menu closes and reactivate
        // typeahead when the menu closes, so that we do not have dirty state of
        // `sub-menu > menu-item[selected]` when we reopen.
        //
        // This cannot happen in `close()` because the menu may close via other
        // means Additionally, this cannot happen in onCloseSubmenu because
        // `close-menu` may not be called via focusout of outside click and not
        // triggered by an item
        menu.addEventListener('closed', () => {
            this.item.ariaExpanded = 'false';
            this.dispatchEvent(createActivateTypeaheadEvent());
            this.dispatchEvent(createDeactivateItemsEvent());
            // aria-hidden required so ChromeVox doesn't announce the closed menu
            menu.ariaHidden = 'true';
        }, { once: true });
        // Parent menu is `position: absolute` – this creates a new CSS relative
        // positioning context (similar to doing `position: relative`), so the
        // submenu's `<md-menu slot="submenu" positioning="document">` would be
        // wrong even if we change `md-sub-menu` from `position: relative` to
        // `position: static` because the submenu it would still be positioning
        // itself relative to the parent menu.
        if (menu.positioning === 'document') {
            menu.positioning = 'absolute';
        }
        menu.quick = true;
        // Submenus are in overflow when not fixed. Can remove once we have native
        // popup support
        menu.hasOverflow = true;
        menu.anchorCorner = this.anchorCorner;
        menu.menuCorner = this.menuCorner;
        menu.anchorElement = this.item;
        menu.defaultFocus = 'first-item';
        // aria-hidden management required so ChromeVox doesn't announce the closed
        // menu. Remove it here since we are about to show and focus it.
        menu.removeAttribute('aria-hidden');
        // This is required in the case where we have a leaf menu open and and the
        // user hovers a parent menu's item which is not an md-sub-menu item.
        // If this were set to true, then the menu would close and focus would be
        // lost. That means the focusout event would have a `relatedTarget` of
        // `null` since nothing in the menu would be focused anymore due to the
        // leaf menu closing. restoring focus ensures that we keep focus in the
        // submenu tree.
        menu.skipRestoreFocus = false;
        // Menu could already be opened because of mouse interaction
        const menuAlreadyOpen = menu.open;
        menu.show();
        this.item.ariaExpanded = 'true';
        this.item.ariaHasPopup = 'menu';
        if (menu.id) {
            this.item.setAttribute('aria-controls', menu.id);
        }
        // Deactivate other items. This can be the case if the user has tabbed
        // around the menu and then mouses over an md-sub-menu.
        this.dispatchEvent(createDeactivateItemsEvent());
        this.dispatchEvent(createDeactivateTypeaheadEvent());
        this.item.selected = true;
        // This is the case of mouse hovering when already opened via keyboard or
        // vice versa
        if (!menuAlreadyOpen) {
            let open = (value) => { };
            const opened = new Promise((resolve) => {
                open = resolve;
            });
            menu.addEventListener('opened', open, { once: true });
            await opened;
        }
    }
    /**
     * Closes the submenu.
     */
    async close() {
        const menu = this.menu;
        if (!menu || !menu.open)
            return;
        this.dispatchEvent(createActivateTypeaheadEvent());
        menu.quick = true;
        menu.close();
        this.dispatchEvent(createDeactivateItemsEvent());
        let close = (value) => { };
        const closed = new Promise((resolve) => {
            close = resolve;
        });
        menu.addEventListener('closed', close, { once: true });
        await closed;
    }
    onSlotchange() {
        if (!this.item) {
            return;
        }
        // TODO(b/301296618): clean up old aria values on change
        this.item.ariaExpanded = 'false';
        this.item.ariaHasPopup = 'menu';
        if (this.menu?.id) {
            this.item.setAttribute('aria-controls', this.menu.id);
        }
        this.item.keepOpen = true;
        const menu = this.menu;
        if (!menu)
            return;
        menu.isSubmenu = true;
        // Required for ChromeVox to not linearly navigate to the menu while closed
        menu.ariaHidden = 'true';
    }
    onClick() {
        this.show();
    }
    /**
     * On item keydown handles opening the submenu.
     */
    async onKeydown(event) {
        const shouldOpenSubmenu = this.isSubmenuOpenKey(event.code);
        if (event.defaultPrevented)
            return;
        const openedWithLR = shouldOpenSubmenu &&
            (NavigableKey.LEFT === event.code || NavigableKey.RIGHT === event.code);
        if (event.code === SelectionKey.SPACE || openedWithLR) {
            // prevent space from scrolling and Left + Right from selecting previous /
            // next items or opening / closing parent menus. Only open the submenu.
            event.preventDefault();
            if (openedWithLR) {
                event.stopPropagation();
            }
        }
        if (!shouldOpenSubmenu) {
            return;
        }
        const submenu = this.menu;
        if (!submenu)
            return;
        const submenuItems = submenu.items;
        const firstActivatableItem = getFirstActivatableItem(submenuItems);
        if (firstActivatableItem) {
            await this.show();
            firstActivatableItem.tabIndex = 0;
            firstActivatableItem.focus();
            return;
        }
    }
    onCloseSubmenu(event) {
        const { itemPath, reason } = event.detail;
        itemPath.push(this.item);
        this.dispatchEvent(createActivateTypeaheadEvent());
        // Escape should only close one menu not all of the menus unlike space or
        // click selection which should close all menus.
        if (reason.kind === CloseReason.KEYDOWN &&
            reason.key === KeydownCloseKey.ESCAPE) {
            event.stopPropagation();
            this.item.dispatchEvent(createRequestActivationEvent());
            return;
        }
        this.dispatchEvent(createDeactivateItemsEvent());
    }
    async onSubMenuKeydown(event) {
        if (event.defaultPrevented)
            return;
        const { close: shouldClose, keyCode } = this.isSubmenuCloseKey(event.code);
        if (!shouldClose)
            return;
        // Communicate that it's handled so that we don't accidentally close every
        // parent menu. Additionally, we want to isolate things like the typeahead
        // keydowns from bubbling up to the parent menu and confounding things.
        event.preventDefault();
        if (keyCode === NavigableKey.LEFT || keyCode === NavigableKey.RIGHT) {
            // Prevent this from bubbling to parents
            event.stopPropagation();
        }
        await this.close();
        deactivateActiveItem(this.menu.items);
        this.item?.focus();
        this.item.tabIndex = 0;
        this.item.focus();
    }
    /**
     * Determines whether the given KeyboardEvent code is one that should open
     * the submenu. This is RTL-aware. By default, left, right, space, or enter.
     *
     * @param code The native KeyboardEvent code.
     * @return Whether or not the key code should open the submenu.
     */
    isSubmenuOpenKey(code) {
        const isRtl = getComputedStyle(this).direction === 'rtl';
        const arrowEnterKey = isRtl ? NavigableKey.LEFT : NavigableKey.RIGHT;
        switch (code) {
            case arrowEnterKey:
            case SelectionKey.SPACE:
            case SelectionKey.ENTER:
                return true;
            default:
                return false;
        }
    }
    /**
     * Determines whether the given KeyboardEvent code is one that should close
     * the submenu. This is RTL-aware. By default right, left, or escape.
     *
     * @param code The native KeyboardEvent code.
     * @return Whether or not the key code should close the submenu.
     */
    isSubmenuCloseKey(code) {
        const isRtl = getComputedStyle(this).direction === 'rtl';
        const arrowEnterKey = isRtl ? NavigableKey.RIGHT : NavigableKey.LEFT;
        switch (code) {
            case arrowEnterKey:
            case KeydownCloseKey.ESCAPE:
                return { close: true, keyCode: code };
            default:
                return { close: false };
        }
    }
}
__decorate([
    property({ attribute: 'anchor-corner' })
], SubMenu.prototype, "anchorCorner", void 0);
__decorate([
    property({ attribute: 'menu-corner' })
], SubMenu.prototype, "menuCorner", void 0);
__decorate([
    property({ type: Number, attribute: 'hover-open-delay' })
], SubMenu.prototype, "hoverOpenDelay", void 0);
__decorate([
    property({ type: Number, attribute: 'hover-close-delay' })
], SubMenu.prototype, "hoverCloseDelay", void 0);
__decorate([
    property({ type: Boolean, reflect: true, attribute: 'md-sub-menu' })
], SubMenu.prototype, "isSubMenu", void 0);
__decorate([
    queryAssignedElements({ slot: 'item', flatten: true })
], SubMenu.prototype, "items", void 0);
__decorate([
    queryAssignedElements({ slot: 'menu', flatten: true })
], SubMenu.prototype, "menus", void 0);
//# sourceMappingURL=sub-menu.js.map