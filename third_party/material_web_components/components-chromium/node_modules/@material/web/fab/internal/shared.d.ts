/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../elevation/elevation.js';
import '../../focus/md-focus-ring.js';
import '../../ripple/ripple.js';
import { LitElement } from 'lit';
/**
 * Sizes variants available to non-extended FABs.
 */
export type FabSize = 'medium' | 'small' | 'large';
declare const fabBaseClass: import("../../labs/behaviors/mixin.js").MixinReturn<typeof LitElement>;
export declare abstract class SharedFab extends fabBaseClass {
    /** @nocollapse */
    static shadowRootOptions: ShadowRootInit;
    /**
     * The size of the FAB.
     *
     * NOTE: Branded FABs cannot be sized to `small`, and Extended FABs do not
     * have different sizes.
     */
    size: FabSize;
    /**
     * The text to display on the FAB.
     */
    label: string;
    /**
     * Lowers the FAB's elevation.
     */
    lowered: boolean;
    protected render(): import("lit-html").TemplateResult<1>;
    protected getRenderClasses(): {
        lowered: boolean;
        small: boolean;
        large: boolean;
        extended: boolean;
    };
    private renderTouchTarget;
    private renderLabel;
    private renderIcon;
}
export {};
