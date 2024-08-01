/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../../focus/md-focus-ring.js';
import '../../../ripple/ripple.js';
import '../../badge/badge.js';
import { LitElement, PropertyValues } from 'lit';
import { NavigationTabState } from './state.js';
declare const navigationTabBaseClass: import("../../behaviors/mixin.js").MixinReturn<typeof LitElement>;
/**
 * b/265346501 - add docs
 *
 * @fires navigation-tab-rendered {Event} Dispatched when the navigation tab's
 * DOM has rendered and custom element definition has loaded. --bubbles
 * --composed
 * @fires navigation-tab-interaction {CustomEvent<{state: MdNavigationTab}>}
 * Dispatched when the navigation tab has been clicked. --bubbles --composed
 */
export declare class NavigationTab extends navigationTabBaseClass implements NavigationTabState {
    disabled: boolean;
    active: boolean;
    hideInactiveLabel: boolean;
    label?: string;
    badgeValue: string;
    showBadge: boolean;
    buttonElement: HTMLElement | null;
    protected render(): import("lit-html").TemplateResult<1>;
    private getRenderClasses;
    private renderBadge;
    private renderLabel;
    firstUpdated(changedProperties: PropertyValues): void;
    focus(): void;
    blur(): void;
    handleClick(): void;
}
export {};
