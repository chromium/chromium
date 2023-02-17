/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { MDCChipActionFocusBehavior, MDCChipActionType } from '../../action/lib/constants.js';
import { MDCChipAnimation } from '../../chip/lib/constants.js';
import { MDCChipSetAttributes, MDCChipSetEvents } from './constants.js';
/**
 * Defines the shape of the adapter expected by the foundation.
 * Implement this adapter for your framework of choice to delegate updates to
 * the component in your framework of choice. See architecture documentation
 * for more details.
 * https://github.com/material-components/material-components-web/blob/master/docs/code/architecture.md
 */
export interface MDCChipSetAdapter {
    /** Announces the message via an aria-live region */
    announceMessage(message: string): void;
    /** Emits the given event with the given detail. */
    emitEvent<D extends object>(eventName: MDCChipSetEvents, eventDetail: D): void;
    /** Returns the value for the given attribute, if it exists. */
    getAttribute(attrName: MDCChipSetAttributes): string | null;
    /** Returns the actions provided by the child chip at the given index. */
    getChipActionsAtIndex(index: number): MDCChipActionType[];
    /** Returns the number of child chips. */
    getChipCount(): number;
    /** Returns the ID of the chip at the given index. */
    getChipIdAtIndex(index: number): string;
    /** Returns the index of the child chip with the matching ID. */
    getChipIndexById(chipID: string): number;
    /** Proxies to the MDCChip#isActionFocusable method. */
    isChipFocusableAtIndex(index: number, actionType: MDCChipActionType): boolean;
    /** Proxies to the MDCChip#isActionSelectable method. */
    isChipSelectableAtIndex(index: number, actionType: MDCChipActionType): boolean;
    /** Proxies to the MDCChip#isActionSelected method. */
    isChipSelectedAtIndex(index: number, actionType: MDCChipActionType): boolean;
    /** Removes the chip at the given index. */
    removeChipAtIndex(index: number): void;
    /** Proxies to the MDCChip#setActionFocus method. */
    setChipFocusAtIndex(index: number, action: MDCChipActionType, focus: MDCChipActionFocusBehavior): void;
    /** Proxies to the MDCChip#setActionSelected method. */
    setChipSelectedAtIndex(index: number, actionType: MDCChipActionType, isSelected: boolean): void;
    /** Starts the chip animation at the given index. */
    startChipAnimationAtIndex(index: number, animation: MDCChipAnimation): void;
}
