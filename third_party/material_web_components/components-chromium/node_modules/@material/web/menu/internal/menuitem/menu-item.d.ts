/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../../focus/md-focus-ring.js';
import '../../../labs/item/item.js';
import '../../../ripple/ripple.js';
import { LitElement, nothing, TemplateResult } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
import { MenuItem, type MenuItemType } from '../controllers/menuItemController.js';
declare const menuItemBaseClass: import("../../../labs/behaviors/mixin.js").MixinReturn<typeof LitElement>;
/**
 * @fires close-menu {CustomEvent<{initiator: SelectOption, reason: Reason, itemPath: SelectOption[]}>}
 * Closes the encapsulating menu on closable interaction. --bubbles --composed
 */
export declare class MenuItemEl extends menuItemBaseClass implements MenuItem {
    /** @nocollapse */
    static shadowRootOptions: {
        delegatesFocus: boolean;
        mode: ShadowRootMode;
        serializable?: boolean;
        slotAssignment?: SlotAssignmentMode;
    };
    /**
     * Disables the item and makes it non-selectable and non-interactive.
     */
    disabled: boolean;
    /**
     * Sets the behavior and role of the menu item, defaults to "menuitem".
     */
    type: MenuItemType;
    /**
     * Sets the underlying `HTMLAnchorElement`'s `href` resource attribute.
     */
    href: string;
    /**
     * Sets the underlying `HTMLAnchorElement`'s `target` attribute when `href` is
     * set.
     */
    target: '_blank' | '_parent' | '_self' | '_top' | '';
    /**
     * Keeps the menu open if clicked or keyboard selected.
     */
    keepOpen: boolean;
    /**
     * Sets the item in the selected visual state when a submenu is opened.
     */
    selected: boolean;
    protected readonly listItemRoot: HTMLElement | null;
    protected readonly headlineElements: HTMLElement[];
    protected readonly supportingTextElements: HTMLElement[];
    protected readonly defaultElements: Node[];
    /**
     * The text that is selectable via typeahead. If not set, defaults to the
     * innerText of the item slotted into the `"headline"` slot.
     */
    get typeaheadText(): string;
    set typeaheadText(text: string);
    private readonly menuItemController;
    protected render(): TemplateResult;
    /**
     * Renders the root list item.
     *
     * @param content the child content of the list item.
     */
    protected renderListItem(content: unknown): TemplateResult;
    /**
     * Handles rendering of the ripple element.
     */
    protected renderRipple(): TemplateResult | typeof nothing;
    /**
     * Handles rendering of the focus ring.
     */
    protected renderFocusRing(): TemplateResult | typeof nothing;
    /**
     * Classes applied to the list item root.
     */
    protected getRenderClasses(): ClassInfo;
    /**
     * Handles rendering the headline and supporting text.
     */
    protected renderBody(): TemplateResult<1>;
    focus(): void;
}
export {};
