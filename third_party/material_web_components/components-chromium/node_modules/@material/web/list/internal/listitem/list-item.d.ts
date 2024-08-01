/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../../focus/md-focus-ring.js';
import '../../../labs/item/item.js';
import '../../../ripple/ripple.js';
import { LitElement, nothing, PropertyValues, TemplateResult } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
import { ListItem } from '../list-navigation-helpers.js';
/**
 * Supported behaviors for a list item.
 */
export type ListItemType = 'text' | 'button' | 'link';
declare const listItemBaseClass: import("../../../labs/behaviors/mixin.js").MixinReturn<typeof LitElement>;
/**
 * @fires request-activation {Event} Requests the list to set `tabindex=0` on
 * the item and focus it. --bubbles --composed
 */
export declare class ListItemEl extends listItemBaseClass implements ListItem {
    /** @nocollapse */
    static shadowRootOptions: {
        delegatesFocus: boolean;
        mode: ShadowRootMode;
        slotAssignment?: SlotAssignmentMode;
    };
    /**
     * Disables the item and makes it non-selectable and non-interactive.
     */
    disabled: boolean;
    /**
     * Sets the behavior of the list item, defaults to "text". Change to "link" or
     * "button" for interactive items.
     */
    type: ListItemType;
    /**
     * READONLY. Sets the `md-list-item` attribute on the element.
     */
    isListItem: boolean;
    /**
     * Sets the underlying `HTMLAnchorElement`'s `href` resource attribute.
     */
    href: string;
    /**
     * Sets the underlying `HTMLAnchorElement`'s `target` attribute when `href` is
     * set.
     */
    target: '_blank' | '_parent' | '_self' | '_top' | '';
    protected readonly listItemRoot: HTMLElement | null;
    private get isDisabled();
    protected willUpdate(changed: PropertyValues<ListItemEl>): void;
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
    protected onFocusRingVisibilityChanged(e: Event): void;
    /**
     * Classes applied to the list item root.
     */
    protected getRenderClasses(): ClassInfo;
    /**
     * Handles rendering the headline and supporting text.
     */
    protected renderBody(): TemplateResult<1>;
    protected onFocus(): void;
    focus(): void;
}
export {};
