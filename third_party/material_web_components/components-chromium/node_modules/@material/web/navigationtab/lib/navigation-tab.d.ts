/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../badge/badge.js';
import '../../focus/focus-ring.js';
import { PropertyValues, TemplateResult } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
import { ActionElement, BeginPressConfig, EndPressConfig } from '../../actionelement/action-element.js';
import { MdRipple } from '../../ripple/ripple.js';
import { NavigationTabState } from './state.js';
/** @soyCompatible */
export declare class NavigationTab extends ActionElement implements NavigationTabState {
    disabled: boolean;
    active: boolean;
    hideInactiveLabel: boolean;
    label?: string;
    badgeValue: string;
    showBadge: boolean;
    protected showFocusRing: boolean;
    ariaLabel: string;
    buttonElement: HTMLElement;
    ripple: MdRipple;
    /** @soyTemplate */
    render(): TemplateResult;
    /** @soyTemplate */
    protected getRenderClasses(): ClassInfo;
    /** @soyTemplate */
    protected renderFocusRing(): TemplateResult;
    /** @soyTemplate */
    protected renderRipple(): TemplateResult | string;
    /** @soyTemplate */
    protected renderBadge(): TemplateResult | '';
    /** @soyTemplate */
    protected renderLabel(): TemplateResult | '';
    firstUpdated(changedProperties: PropertyValues): void;
    focus(): void;
    blur(): void;
    beginPress({ positionEvent }: BeginPressConfig): void;
    endPress(options: EndPressConfig): void;
    handlePointerDown(e: PointerEvent): void;
    handlePointerUp(e: PointerEvent): void;
    protected handlePointerEnter(e: PointerEvent): void;
    handlePointerLeave(e: PointerEvent): void;
    protected handleFocus(): void;
    protected handleBlur(): void;
}
