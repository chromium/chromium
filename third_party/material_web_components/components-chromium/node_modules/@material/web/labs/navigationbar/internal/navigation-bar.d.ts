/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../../elevation/elevation.js';
import { LitElement, PropertyValues } from 'lit';
import { NavigationTab } from '../../navigationtab/internal/navigation-tab.js';
import { NavigationBarState } from './state.js';
declare const navigationBarBaseClass: import("../../behaviors/mixin.js").MixinReturn<typeof LitElement>;
/**
 * b/265346501 - add docs
 *
 * @fires navigation-bar-activated {CustomEvent<tab: NavigationTab, activeIndex: number>}
 * Dispatched whenever the `activeIndex` changes. --bubbles --composed
 */
export declare class NavigationBar extends navigationBarBaseClass implements NavigationBarState {
    activeIndex: number;
    hideInactiveLabels: boolean;
    tabs: NavigationTab[];
    private readonly tabsElement;
    protected render(): import("lit-html").TemplateResult<1>;
    protected updated(changedProperties: PropertyValues<NavigationBar>): void;
    firstUpdated(changedProperties: PropertyValues): void;
    layout(): void;
    private handleNavigationTabConnected;
    private handleNavigationTabInteraction;
    private handleKeydown;
    private onActiveIndexChange;
    private onHideInactiveLabelsChange;
}
export {};
