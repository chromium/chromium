/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { MDCChipActionInteractionTrigger, MDCChipActionType } from './constants.js';
/**
 * MDCChipActionInteractionEventDetail provides the details for the interaction
 * event.
 */
export interface MDCChipActionInteractionEventDetail {
    actionID: string;
    source: MDCChipActionType;
    trigger: MDCChipActionInteractionTrigger;
}
/**
 * MDCChipActionNavigationEventDetail provides the details for the navigation
 * event.
 */
export interface MDCChipActionNavigationEventDetail {
    source: MDCChipActionType;
    key: string;
}
