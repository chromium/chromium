/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../elevation/elevation.js';
import '../../focus/md-focus-ring.js';
import '../../ripple/ripple.js';
import { LitElement } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
/**
 * Symbol for tabs to use to animate their indicators based off another tab's
 * indicator.
 */
declare const INDICATOR: unique symbol;
/**
 * Symbol used by the tab bar to request a tab to animate its indicator from a
 * previously selected tab.
 */
export declare const ANIMATE_INDICATOR: unique symbol;
declare const tabBaseClass: import("../../labs/behaviors/mixin.js").MixinReturn<typeof LitElement, import("../../labs/behaviors/focusable.js").Focusable>;
/**
 * Tab component.
 */
export declare class Tab extends tabBaseClass {
    /**
     * The attribute `md-tab` indicates that the element is a tab for the parent
     * element, `<md-tabs>`. Make sure if you're implementing your own `md-tab`
     * component that you have an `md-tab` attribute set.
     */
    readonly isTab = true;
    /**
     * Whether or not the tab is selected.
     **/
    active: boolean;
    /**
     * @deprecated use `active`
     */
    get selected(): boolean;
    set selected(active: boolean);
    /**
     * In SSR, set this to true when an icon is present.
     */
    hasIcon: boolean;
    /**
     * In SSR, set this to true when there is no label and only an icon.
     */
    iconOnly: boolean;
    readonly [INDICATOR]: HTMLElement | null;
    protected fullWidthIndicator: boolean;
    private readonly assignedDefaultNodes;
    private readonly assignedIcons;
    private readonly internals;
    constructor();
    protected render(): import("lit-html").TemplateResult<1>;
    protected getContentClasses(): ClassInfo;
    protected updated(): void;
    private handleKeydown;
    private handleContentClick;
    [ANIMATE_INDICATOR](previousTab: Tab): void;
    private getKeyframes;
    private handleSlotChange;
    private handleIconSlotChange;
}
export {};
