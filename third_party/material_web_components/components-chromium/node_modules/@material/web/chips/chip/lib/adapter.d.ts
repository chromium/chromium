/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { MDCChipActionFocusBehavior, MDCChipActionType } from '../../action/lib/constants.js';
import { MDCChipAttributes, MDCChipCssClasses, MDCChipEvents } from './constants.js';
/**
 * Defines the shape of the adapter expected by the foundation.
 * Implement this adapter for your framework of choice to delegate updates to
 * the component in your framework of choice. See architecture documentation
 * for more details.
 * https://github.com/material-components/material-components-web/blob/master/docs/code/architecture.md
 */
export interface MDCChipAdapter {
    /** Adds the given class to the root element. */
    addClass(className: MDCChipCssClasses): void;
    /** Emits the given event with the given detail. */
    emitEvent<D extends object>(eventName: MDCChipEvents, eventDetail: D): void;
    /** Returns the child actions provided by the chip. */
    getActions(): MDCChipActionType[];
    /** Returns the value for the given attribute, if it exists. */
    getAttribute(attrName: MDCChipAttributes): string | null;
    /** Returns the ID of the root element. */
    getElementID(): string;
    /** Returns the offset width of the root element. */
    getOffsetWidth(): number;
    /** Returns true if the root element has the given class. */
    hasClass(className: MDCChipCssClasses): boolean;
    /** Proxies to the MDCChipAction#isSelectable method. */
    isActionSelectable(action: MDCChipActionType): boolean;
    /** Proxies to the MDCChipAction#isSelected method. */
    isActionSelected(action: MDCChipActionType): boolean;
    /** Proxies to the MDCChipAction#isFocusable method. */
    isActionFocusable(action: MDCChipActionType): boolean;
    /** Proxies to the MDCChipAction#isDisabled method. */
    isActionDisabled(action: MDCChipActionType): boolean;
    /** Returns true if the text direction is right-to-left. */
    isRTL(): boolean;
    /** Removes the given class from the root element. */
    removeClass(className: MDCChipCssClasses): void;
    /** Proxies to the MDCChipAction#setDisabled method. */
    setActionDisabled(action: MDCChipActionType, isDisabled: boolean): void;
    /** Proxies to the MDCChipAction#setFocus method. */
    setActionFocus(action: MDCChipActionType, behavior: MDCChipActionFocusBehavior): void;
    /** Proxies to the MDCChipAction#setSelected method. */
    setActionSelected(action: MDCChipActionType, isSelected: boolean): void;
    /** Sets the style property to the given value. */
    setStyleProperty(property: string, value: string): void;
}
