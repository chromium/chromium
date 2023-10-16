/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { NavigationTabState } from '../../navigationtab/internal/state.js';
/**
 * MDCNavigationTabInteractionEventDetail provides details for the interaction
 * event.
 */
export interface MDCNavigationTabInteractionEventDetail {
    state: NavigationTabState;
}
/**
 * NavigationTabInteractionEvent is the custom event for the interaction event.
 */
export type NavigationTabInteractionEvent = CustomEvent<MDCNavigationTabInteractionEventDetail>;
