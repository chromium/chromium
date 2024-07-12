/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * The state of a navigation tab.
 */
export interface NavigationTabState {
    /**
     * Active state of the navigation tab.
     */
    active: boolean;
    /**
     * If true, when inactive label will be hidden.
     */
    hideInactiveLabel: boolean;
}
