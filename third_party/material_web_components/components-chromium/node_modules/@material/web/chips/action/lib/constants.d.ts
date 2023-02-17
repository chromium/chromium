/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * MDCChipActionCssClasses provides the classes to be queried and manipulated on
 * the root.
 */
export declare enum MDCChipActionCssClasses {
    PRIMARY_ACTION = "md3-evolution-chip__action--primary",
    TRAILING_ACTION = "md3-evolution-chip__action--trailing",
    CHIP_ROOT = "md3-evolution-chip"
}
/**
 * MDCChipActionInteractionTrigger provides detail of the different triggers for
 * action interactions.
 */
export declare enum MDCChipActionInteractionTrigger {
    UNSPECIFIED = 0,
    CLICK = 1,
    BACKSPACE_KEY = 2,
    DELETE_KEY = 3,
    SPACEBAR_KEY = 4,
    ENTER_KEY = 5
}
/**
 * MDCChipActionType provides the different types of available actions.
 */
export declare enum MDCChipActionType {
    UNSPECIFIED = 0,
    PRIMARY = 1,
    TRAILING = 2
}
/**
 * MDCChipActionEvents provides the different events emitted by the action.
 */
export declare enum MDCChipActionEvents {
    INTERACTION = "MDCChipAction:interaction",
    NAVIGATION = "MDCChipAction:navigation"
}
/**
 * MDCChipActionFocusBehavior provides configurations for focusing or unfocusing
 * an action.
 */
export declare enum MDCChipActionFocusBehavior {
    FOCUSABLE = 0,
    FOCUSABLE_AND_FOCUSED = 1,
    NOT_FOCUSABLE = 2
}
/**
 * MDCChipActionAttributes provides the HTML attributes used by the foundation.
 */
export declare enum MDCChipActionAttributes {
    ARIA_DISABLED = "aria-disabled",
    ARIA_HIDDEN = "aria-hidden",
    ARIA_SELECTED = "aria-selected",
    DATA_DELETABLE = "data-mdc-deletable",
    DISABLED = "disabled",
    ROLE = "role",
    TAB_INDEX = "tabindex"
}
