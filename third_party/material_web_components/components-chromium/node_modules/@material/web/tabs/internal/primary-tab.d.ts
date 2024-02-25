/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Tab } from './tab.js';
/**
 * A primary tab component.
 */
export declare class PrimaryTab extends Tab {
    /**
     * Whether or not the icon renders inline with label or stacked vertically.
     */
    inlineIcon: boolean;
    protected getContentClasses(): {
        stacked: boolean;
    };
}
