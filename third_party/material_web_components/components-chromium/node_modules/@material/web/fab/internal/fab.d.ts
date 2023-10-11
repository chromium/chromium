/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { SharedFab } from './shared.js';
/**
 * The variants available to non-branded FABs.
 */
export type FabVariant = 'surface' | 'primary' | 'secondary' | 'tertiary';
export declare class Fab extends SharedFab {
    /**
     * The FAB color variant to render.
     */
    variant: FabVariant;
    protected getRenderClasses(): {
        primary: boolean;
        secondary: boolean;
        tertiary: boolean;
        lowered: boolean;
        small: boolean;
        large: boolean;
        extended: boolean;
    };
}
