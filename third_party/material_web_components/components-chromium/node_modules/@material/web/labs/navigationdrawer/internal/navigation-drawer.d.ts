/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../../elevation/elevation.js';
import { LitElement, PropertyValues } from 'lit';
declare const navigationDrawerBaseClass: import("../../behaviors/mixin.js").MixinReturn<typeof LitElement>;
/**
 * b/265346501 - add docs
 *
 * @fires navigation-drawer-changed {CustomEvent<{opened: boolean}>}
 * Dispatched whenever the drawer opens or closes --bubbles --composed
 */
export declare class NavigationDrawer extends navigationDrawerBaseClass {
    opened: boolean;
    pivot: 'start' | 'end';
    protected render(): import("lit-html").TemplateResult<1>;
    private getRenderClasses;
    protected updated(changedProperties: PropertyValues<NavigationDrawer>): void;
}
export {};
