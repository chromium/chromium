/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { MenuItem } from './menuItemController.js';
/**
 * The options that are passed to the typeahead controller.
 */
export interface TypeaheadControllerProperties {
    /**
     * A function that returns an array of menu items to be searched.
     * @return An array of menu items to be searched by typing.
     */
    getItems: () => MenuItem[];
    /**
     * The maximum time between each keystroke to keep the current type buffer
     * alive.
     */
    typeaheadBufferTime: number;
    /**
     * Whether or not the typeahead should listen for keystrokes or not.
     */
    active: boolean;
}
/**
 * Data structure tuple that helps with indexing.
 *
 * [index, item, normalized header text]
 */
type TypeaheadRecord = [number, MenuItem, string];
/**
 * Indicies to access the TypeaheadRecord tuple type.
 */
export declare const TYPEAHEAD_RECORD: {
    readonly INDEX: 0;
    readonly ITEM: 1;
    readonly TEXT: 2;
};
/**
 * This controller listens to `keydown` events and searches the header text of
 * an array of `MenuItem`s with the corresponding entered keys within the buffer
 * time and activates the item.
 *
 * @example
 * ```ts
 * const typeaheadController = new TypeaheadController(() => ({
 *   typeaheadBufferTime: 50,
 *   getItems: () => Array.from(document.querySelectorAll('md-menu-item'))
 * }));
 * html`
 *   <div
 *       @keydown=${typeaheadController.onKeydown}
 *       tabindex="0"
 *       class="activeItemText">
 *     <!-- focusable element that will receive keydown events -->
 *     Apple
 *   </div>
 *   <div>
 *     <md-menu-item active header="Apple"></md-menu-item>
 *     <md-menu-item header="Apricot"></md-menu-item>
 *     <md-menu-item header="Banana"></md-menu-item>
 *     <md-menu-item header="Olive"></md-menu-item>
 *     <md-menu-item header="Orange"></md-menu-item>
 *   </div>
 * `;
 * ```
 */
export declare class TypeaheadController {
    private readonly getProperties;
    /**
     * Array of tuples that helps with indexing.
     */
    private typeaheadRecords;
    /**
     * Currently-typed text since last buffer timeout
     */
    private typaheadBuffer;
    /**
     * The timeout id from the current buffer's setTimeout
     */
    private cancelTypeaheadTimeout;
    /**
     * If we are currently "typing"
     */
    isTypingAhead: boolean;
    /**
     * The record of the last active item.
     */
    lastActiveRecord: TypeaheadRecord | null;
    /**
     * @param getProperties A function that returns the options of the typeahead
     * controller:
     *
     * {
     *   getItems: A function that returns an array of menu items to be searched.
     *   typeaheadBufferTime: The maximum time between each keystroke to keep the
     *       current type buffer alive.
     * }
     */
    constructor(getProperties: () => TypeaheadControllerProperties);
    private get items();
    private get active();
    /**
     * Apply this listener to the element that will receive `keydown` events that
     * should trigger this controller.
     *
     * @param event The native browser `KeyboardEvent` from the `keydown` event.
     */
    readonly onKeydown: (event: KeyboardEvent) => void;
    /**
     * Sets up typingahead
     */
    private beginTypeahead;
    /**
     * Performs the typeahead. Based on the normalized items and the current text
     * buffer, finds the _next_ item with matching text and activates it.
     *
     * @example
     *
     * items: Apple, Banana, Olive, Orange, Cucumber
     * buffer: ''
     * user types: o
     *
     * activates Olive
     *
     * @example
     *
     * items: Apple, Banana, Olive (active), Orange, Cucumber
     * buffer: 'o'
     * user types: l
     *
     * activates Olive
     *
     * @example
     *
     * items: Apple, Banana, Olive (active), Orange, Cucumber
     * buffer: ''
     * user types: o
     *
     * activates Orange
     *
     * @example
     *
     * items: Apple, Banana, Olive, Orange (active), Cucumber
     * buffer: ''
     * user types: o
     *
     * activates Olive
     */
    private typeahead;
    /**
     * Ends the current typeahead and clears the buffer.
     */
    private readonly endTypeahead;
}
export {};
