/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { MDCChipAnimationEventDetail, MDCChipInteractionEventDetail, MDCChipNavigationEventDetail } from '../../chip/lib/types.js';
/**
 * MDCChipSetInteractionEventDetail provides detail about the interaction event.
 */
export interface MDCChipSetInteractionEventDetail {
    chipID: string;
    chipIndex: number;
}
/**
 * MDCChipSetRemovalEventDetail provides detail about the removal event.
 */
export interface MDCChipSetRemovalEventDetail {
    chipID: string;
    chipIndex: number;
    isComplete: boolean;
}
/**
 * MDCChipSetSelectionEventDetail provides detail about the selection event.
 */
export interface MDCChipSetSelectionEventDetail {
    chipID: string;
    chipIndex: number;
    isSelected: boolean;
}
/**
 * ChipInteractionEvent is the custom event for the interaction event.
 */
export declare type ChipInteractionEvent = CustomEvent<MDCChipInteractionEventDetail>;
/**
 * ChipNavigationEvent is the custom event for the navigation event.
 */
export declare type ChipNavigationEvent = CustomEvent<MDCChipNavigationEventDetail>;
/**
 * ChipAnimationEvent is the custom event for the animation event.
 */
export declare type ChipAnimationEvent = CustomEvent<MDCChipAnimationEventDetail>;
