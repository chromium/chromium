/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { ReactiveController, ReactiveControllerHost } from 'lit';
import { MenuItemControllerConfig } from '../../../menu/internal/controllers/menuItemController.js';
import { SelectOption } from './select-option.js';
/**
 * Creates an event fired by a SelectOption to request selection from md-select.
 * Typically fired after `selected` changes from `false` to `true`.
 */
export declare function createRequestSelectionEvent(): Event;
/**
 * Creates an event fired by a SelectOption to request deselection from
 * md-select. Typically fired after `selected` changes from `true` to `false`.
 */
export declare function createRequestDeselectionEvent(): Event;
/**
 * The options used to inialize SelectOptionController.
 */
export type SelectOptionConfig = MenuItemControllerConfig;
/**
 * A controller that provides most functionality and md-select compatibility for
 * an element that implements the SelectOption interface.
 */
export declare class SelectOptionController implements ReactiveController {
    private readonly host;
    private readonly menuItemController;
    private internalDisplayText;
    private lastSelected;
    private firstUpdate;
    /**
     * The recommended role of the select option.
     */
    get role(): "menuitem" | "option";
    /**
     * The text that is selectable via typeahead. If not set, defaults to the
     * innerText of the item slotted into the `"headline"` slot, and if there are
     * no slotted elements into headline, then it checks the _default_ slot, and
     * then the `"supporting-text"` slot if nothing is in _default_.
     */
    get typeaheadText(): string;
    setTypeaheadText(text: string): void;
    /**
     * The text that is displayed in the select field when selected. If not set,
     * defaults to the textContent of the item slotted into the `"headline"` slot,
     * and if there are no slotted elements into headline, then it checks the
     * _default_ slot, and then the `"supporting-text"` slot if nothing is in
     * _default_.
     */
    get displayText(): string;
    setDisplayText(text: string): void;
    /**
     * @param host The SelectOption in which to attach this controller to.
     * @param config The object that configures this controller's behavior.
     */
    constructor(host: ReactiveControllerHost & SelectOption, config: SelectOptionConfig);
    hostUpdate(): void;
    hostUpdated(): void;
    /**
     * Bind this click listener to the interactive element. Handles closing the
     * menu.
     */
    onClick: () => void;
    /**
     * Bind this click listener to the interactive element. Handles closing the
     * menu.
     */
    onKeydown: (e: KeyboardEvent) => void;
}
