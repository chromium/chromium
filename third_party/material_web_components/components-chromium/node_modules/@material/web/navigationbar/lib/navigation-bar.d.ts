/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../elevation/elevation.js';
import { LitElement, PropertyValues, TemplateResult } from 'lit';
import { NavigationTab } from '../../navigationtab/lib/navigation-tab.js';
import { NavigationBarState } from './state.js';
/** @soyCompatible */
export declare class NavigationBar extends LitElement implements NavigationBarState {
    activeIndex: number;
    hideInactiveLabels: boolean;
    tabs: NavigationTab[];
    protected tabsElement: NavigationTab[];
    ariaLabel: string;
    /** @soyTemplate */
    render(): TemplateResult;
    protected updated(changedProperties: PropertyValues<NavigationBar>): void;
    firstUpdated(changedProperties: PropertyValues): void;
    layout(): void;
    private handleNavigationTabConnected;
    private handleNavigationTabInteraction;
    private handleKeydown;
    private onActiveIndexChange;
    private onHideInactiveLabelsChange;
}
