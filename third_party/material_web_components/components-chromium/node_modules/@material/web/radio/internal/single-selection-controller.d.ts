/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { ReactiveController } from 'lit';
/**
 * An element that supports single-selection with `SingleSelectionController`.
 */
export interface SingleSelectionElement extends HTMLElement {
    /**
     * Whether or not the element is selected.
     */
    checked: boolean;
}
/**
 * A `ReactiveController` that provides root node-scoped single selection for
 * elements, similar to native `<input type="radio">` selection.
 *
 * To use, elements should add the controller and call
 * `selectionController.handleCheckedChange()` in a getter/setter. This must
 * be synchronous to match native behavior.
 *
 * @example
 * const CHECKED = Symbol('checked');
 *
 * class MyToggle extends LitElement {
 *   get checked() { return this[CHECKED]; }
 *   set checked(checked: boolean) {
 *     const oldValue = this.checked;
 *     if (oldValue === checked) {
 *       return;
 *     }
 *
 *     this[CHECKED] = checked;
 *     this.selectionController.handleCheckedChange();
 *     this.requestUpdate('checked', oldValue);
 *   }
 *
 *   [CHECKED] = false;
 *
 *   private selectionController = new SingleSelectionController(this);
 *
 *   constructor() {
 *     super();
 *     this.addController(this.selectionController);
 *   }
 * }
 */
export declare class SingleSelectionController implements ReactiveController {
    private readonly host;
    /**
     * All single selection elements in the host element's root with the same
     * `name` attribute, including the host element.
     */
    get controls(): [SingleSelectionElement, ...SingleSelectionElement[]];
    private focused;
    private root;
    constructor(host: SingleSelectionElement);
    hostConnected(): void;
    hostDisconnected(): void;
    /**
     * Should be called whenever the host's `checked` property changes
     * synchronously.
     */
    handleCheckedChange(): void;
    private readonly handleFocusIn;
    private readonly handleFocusOut;
    private uncheckSiblings;
    /**
     * Updates the `tabindex` of the host and its siblings.
     */
    private updateTabIndices;
    /**
     * Handles arrow key events from the host. Using the arrow keys will
     * select and check the next or previous sibling with the host's
     * `name` attribute.
     */
    private readonly handleKeyDown;
}
