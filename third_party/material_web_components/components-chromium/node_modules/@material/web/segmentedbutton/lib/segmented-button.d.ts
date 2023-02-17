/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../focus/focus-ring.js';
import { PropertyValues, TemplateResult } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
import { ActionElement, BeginPressConfig, EndPressConfig } from '../../actionelement/action-element.js';
import { MdRipple } from '../../ripple/ripple.js';
/**
 * SegmentedButton is a web component implementation of the Material Design
 * segmented button component. It is intended **only** for use as a child of a
 * `SementedButtonSet` component. It is **not** intended for use in any other
 * context.
 * @soyCompatible
 */
export declare class SegmentedButton extends ActionElement {
    disabled: boolean;
    selected: boolean;
    label: string;
    noCheckmark: boolean;
    hasIcon: boolean;
    /** @soyPrefixAttribute */
    ariaLabel: string;
    protected animState: string;
    protected showFocusRing: boolean;
    protected iconElement: HTMLElement[];
    ripple: MdRipple;
    protected update(props: PropertyValues<SegmentedButton>): void;
    private nextAnimationState;
    beginPress({ positionEvent }: BeginPressConfig): void;
    endPress(options: EndPressConfig): void;
    handlePointerDown(e: PointerEvent): void;
    handlePointerUp(e: PointerEvent): void;
    protected handlePointerEnter(e: PointerEvent): void;
    handlePointerLeave(e: PointerEvent): void;
    protected handleFocus(): void;
    protected handleBlur(): void;
    /** @soyTemplate */
    render(): TemplateResult;
    /** @soyTemplate */
    protected getRenderClasses(): ClassInfo;
    /** @soyTemplate */
    protected renderFocusRing(): TemplateResult;
    /** @soyTemplate */
    protected renderRipple(): TemplateResult | string;
    /** @soyTemplate */
    protected renderOutline(): TemplateResult;
    /** @soyTemplate */
    protected renderLeading(): TemplateResult;
    /** @soyTemplate */
    protected renderLeadingWithoutLabel(): TemplateResult;
    /** @soyTemplate */
    protected renderLeadingWithLabel(): TemplateResult;
    /** @soyTemplate */
    protected renderLabel(): TemplateResult;
    /** @soyTemplate */
    protected renderTouchTarget(): TemplateResult;
}
