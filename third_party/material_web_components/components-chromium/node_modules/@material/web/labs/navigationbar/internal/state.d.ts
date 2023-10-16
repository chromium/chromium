/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { NavigationTabState } from '../../navigationtab/internal/state.js';
/**
 * The state of a navigation Bar.
 */
export interface NavigationBarState {
    /**
     * Index of the active navigation tab.
     */
    activeIndex: number;
    /**
     * If true, inactive navigation tabs will hide their label.
     */
    hideInactiveLabels: boolean;
    /**
     * An array of the navigation tab states.
     */
    tabs: NavigationTabState[];
}
