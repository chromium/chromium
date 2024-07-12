/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { CSSResultOrNative } from 'lit';
import { Fab, FabVariant } from './internal/fab.js';
export { type FabSize } from './internal/shared.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-branded-fab': MdBrandedFab;
    }
}
/**
 * @summary Floating action buttons (FABs) help people take primary actions.
 * They’re used to represent the most important action on a screen, such as
 * Create or Reply.
 *
 * @description
 * __Emphasis:__ High emphasis – For the primary, most important, or most common
 * action on a screen
 *
 * __Rationale:__ The FAB remains the default component for a screen’s primary
 * action. It comes in three sizes: small FAB, FAB, and large FAB. The extended
 * FAB’s wider format and text label give it more visual prominence than a  FAB.
 * It’s often used on larger screens where a FAB would seem too small. Branded
 * FABs are used to specifically call attention to branded logo icons.
 *
 * __Example usages:__
 * - FAB
 *   - Create
 *   - Compose
 * - Extended FAB
 *   - Create
 *   - Compose
 *   - New Thread
 *   - New File
 *
 * @final
 * @suppress {visibility}
 */
export declare class MdBrandedFab extends Fab {
    /**
     * Branded FABs have no variants
     */
    variant: FabVariant;
    getRenderClasses(): {
        primary: boolean;
        secondary: boolean;
        tertiary: boolean;
        small: boolean;
        lowered: boolean;
        large: boolean;
        extended: boolean;
    };
    static styles: CSSResultOrNative[];
}
