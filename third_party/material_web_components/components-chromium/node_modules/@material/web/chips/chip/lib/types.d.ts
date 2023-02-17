/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { MDCChipActionType } from '../../action/lib/constants.js';
import { MDCChipActionInteractionEventDetail, MDCChipActionNavigationEventDetail } from '../../action/lib/types.js';
import { MDCChipAnimation } from './constants.js';
/** MDCChipInteractionEventDetail provides details for the interaction event. */
export interface MDCChipInteractionEventDetail {
    actionID: string;
    chipID: string;
    source: MDCChipActionType;
    shouldRemove: boolean;
    isSelectable: boolean;
    isSelected: boolean;
}
/** MDCChipNavigationEventDetail provides details for the navigation event. */
export interface MDCChipNavigationEventDetail {
    chipID: string;
    source: MDCChipActionType;
    key: string;
    isRTL: boolean;
}
/**
 * MDCChipAnimationEventDetail provides details for the animation event.
 */
export interface MDCChipAnimationEventDetail {
    chipID: string;
    animation: MDCChipAnimation;
    isComplete: boolean;
    addedAnnouncement?: string;
    removedAnnouncement?: string;
}
/**
 * MDCChipActionInteractionEvent is the custom event for the interaction event.
 */
export declare type ActionInteractionEvent = CustomEvent<MDCChipActionInteractionEventDetail>;
/**
 * MDCChipActionInteractionEvent is the custom event for the interaction event.
 */
export declare type ActionNavigationEvent = CustomEvent<MDCChipActionNavigationEventDetail>;
