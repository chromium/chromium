/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * MDCChipActionCssClasses provides the classes to be queried and manipulated on
 * the root.
 */
export var MDCChipActionCssClasses;
(function (MDCChipActionCssClasses) {
    MDCChipActionCssClasses["PRIMARY_ACTION"] = "md3-evolution-chip__action--primary";
    MDCChipActionCssClasses["TRAILING_ACTION"] = "md3-evolution-chip__action--trailing";
    MDCChipActionCssClasses["CHIP_ROOT"] = "md3-evolution-chip";
})(MDCChipActionCssClasses || (MDCChipActionCssClasses = {}));
/**
 * MDCChipActionInteractionTrigger provides detail of the different triggers for
 * action interactions.
 */
export var MDCChipActionInteractionTrigger;
(function (MDCChipActionInteractionTrigger) {
    MDCChipActionInteractionTrigger[MDCChipActionInteractionTrigger["UNSPECIFIED"] = 0] = "UNSPECIFIED";
    MDCChipActionInteractionTrigger[MDCChipActionInteractionTrigger["CLICK"] = 1] = "CLICK";
    MDCChipActionInteractionTrigger[MDCChipActionInteractionTrigger["BACKSPACE_KEY"] = 2] = "BACKSPACE_KEY";
    MDCChipActionInteractionTrigger[MDCChipActionInteractionTrigger["DELETE_KEY"] = 3] = "DELETE_KEY";
    MDCChipActionInteractionTrigger[MDCChipActionInteractionTrigger["SPACEBAR_KEY"] = 4] = "SPACEBAR_KEY";
    MDCChipActionInteractionTrigger[MDCChipActionInteractionTrigger["ENTER_KEY"] = 5] = "ENTER_KEY";
})(MDCChipActionInteractionTrigger || (MDCChipActionInteractionTrigger = {}));
/**
 * MDCChipActionType provides the different types of available actions.
 */
export var MDCChipActionType;
(function (MDCChipActionType) {
    MDCChipActionType[MDCChipActionType["UNSPECIFIED"] = 0] = "UNSPECIFIED";
    MDCChipActionType[MDCChipActionType["PRIMARY"] = 1] = "PRIMARY";
    MDCChipActionType[MDCChipActionType["TRAILING"] = 2] = "TRAILING";
})(MDCChipActionType || (MDCChipActionType = {}));
/**
 * MDCChipActionEvents provides the different events emitted by the action.
 */
export var MDCChipActionEvents;
(function (MDCChipActionEvents) {
    MDCChipActionEvents["INTERACTION"] = "MDCChipAction:interaction";
    MDCChipActionEvents["NAVIGATION"] = "MDCChipAction:navigation";
})(MDCChipActionEvents || (MDCChipActionEvents = {}));
/**
 * MDCChipActionFocusBehavior provides configurations for focusing or unfocusing
 * an action.
 */
export var MDCChipActionFocusBehavior;
(function (MDCChipActionFocusBehavior) {
    MDCChipActionFocusBehavior[MDCChipActionFocusBehavior["FOCUSABLE"] = 0] = "FOCUSABLE";
    MDCChipActionFocusBehavior[MDCChipActionFocusBehavior["FOCUSABLE_AND_FOCUSED"] = 1] = "FOCUSABLE_AND_FOCUSED";
    MDCChipActionFocusBehavior[MDCChipActionFocusBehavior["NOT_FOCUSABLE"] = 2] = "NOT_FOCUSABLE";
})(MDCChipActionFocusBehavior || (MDCChipActionFocusBehavior = {}));
/**
 * MDCChipActionAttributes provides the HTML attributes used by the foundation.
 */
export var MDCChipActionAttributes;
(function (MDCChipActionAttributes) {
    MDCChipActionAttributes["ARIA_DISABLED"] = "aria-disabled";
    MDCChipActionAttributes["ARIA_HIDDEN"] = "aria-hidden";
    MDCChipActionAttributes["ARIA_SELECTED"] = "aria-selected";
    MDCChipActionAttributes["DATA_DELETABLE"] = "data-mdc-deletable";
    MDCChipActionAttributes["DISABLED"] = "disabled";
    MDCChipActionAttributes["ROLE"] = "role";
    MDCChipActionAttributes["TAB_INDEX"] = "tabindex";
})(MDCChipActionAttributes || (MDCChipActionAttributes = {}));
//# sourceMappingURL=constants.js.map