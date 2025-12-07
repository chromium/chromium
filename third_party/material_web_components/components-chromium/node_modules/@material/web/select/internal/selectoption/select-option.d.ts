/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../../focus/md-focus-ring.js';
import '../../../labs/item/item.js';
import '../../../ripple/ripple.js';
import { LitElement } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
import { MenuItem } from '../../../menu/internal/controllers/menuItemController.js';
/**
 * The interface specific to a Select Option
 */
interface SelectOptionSelf {
    /**
     * The form value associated with the Select Option. (Note: the visual portion
     * of the SelectOption is the headline defined in ListItem)
     */
    value: string;
    /**
     * Whether or not the SelectOption is selected.
     */
    selected: boolean;
    /**
     * The text to display in the select when selected. Defaults to the
     * textContent of the Element slotted into the headline.
     */
    displayText: string;
}
/**
 * The interface to implement for a select option. Additionally, the element
 * must have `md-list-item` and `md-menu-item` attributes on the host.
 */
export type SelectOption = SelectOptionSelf & MenuItem;
declare const selectOptionBaseClass: import("../../../labs/behaviors/mixin.js").MixinReturn<typeof LitElement>;
/**
 * @fires close-menu {CustomEvent<{initiator: SelectOption, reason: Reason, itemPath: SelectOption[]}>}
 * Closes the encapsulating menu on closable interaction. --bubbles --composed
 * @fires request-selection {Event} Requests the parent md-select to select this
 * element (and deselect others if single-selection) when `selected` changed to
 * `true`. --bubbles --composed
 * @fires request-deselection {Event} Requests the parent md-select to deselect
 * this element when `selected` changed to `false`. --bubbles --composed
 */
export declare class SelectOptionEl extends selectOptionBaseClass implements SelectOption {
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
     * READONLY: self-identifies as a menu item and sets its identifying attribute
     */
    isMenuItem: boolean;
    /**
     * Sets the item in the selected visual state when a submenu is opened.
     */
    selected: boolean;
    /**
     * Form value of the option.
     */
    value: string;
    protected readonly listItemRoot: HTMLElement | null;
    protected readonly headlineElements: HTMLElement[];
    protected readonly supportingTextElements: HTMLElement[];
    protected readonly defaultElements: Element[];
    type: "option";
    /**
     * The text that is selectable via typeahead. If not set, defaults to the
     * innerText of the item slotted into the `"headline"` slot.
     */
    get typeaheadText(): string;
    set typeaheadText(text: string);
    /**
     * The text that is displayed in the select field when selected. If not set,
     * defaults to the textContent of the item slotted into the `"headline"` slot.
     */
    get displayText(): string;
    set displayText(text: string);
    private readonly selectOptionController;
    protected render(): import("lit-html").TemplateResult<1>;
    /**
     * Renders the root list item.
     *
     * @param content the child content of the list item.
     */
    protected renderListItem(content: unknown): import("lit-html").TemplateResult<1>;
    /**
     * Handles rendering of the ripple element.
     */
    protected renderRipple(): import("lit-html").TemplateResult<1>;
    /**
     * Handles rendering of the focus ring.
     */
    protected renderFocusRing(): import("lit-html").TemplateResult<1>;
    /**
     * Classes applied to the list item root.
     */
    protected getRenderClasses(): ClassInfo;
    /**
     * Handles rendering the headline and supporting text.
     */
    protected renderBody(): import("lit-html").TemplateResult<1>;
    focus(): void;
}
export {};
