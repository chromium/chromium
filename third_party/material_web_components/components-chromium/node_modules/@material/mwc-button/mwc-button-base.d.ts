/**
 * @license
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '@material/mwc-icon/mwc-icon.js';
import '@material/mwc-ripple/mwc-ripple.js';
import { AriaHasPopup } from '@material/mwc-base/aria-property.js';
import { Ripple } from '@material/mwc-ripple/mwc-ripple.js';
import { RippleHandlers } from '@material/mwc-ripple/ripple-handlers.js';
import { LitElement, TemplateResult } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
/** @soyCompatible */
export declare class ButtonBase extends LitElement {
    static shadowRootOptions: ShadowRootInit;
    /** @soyPrefixAttribute */
    ariaHasPopup: AriaHasPopup;
    raised: boolean;
    unelevated: boolean;
    outlined: boolean;
    dense: boolean;
    disabled: boolean;
    trailingIcon: boolean;
    fullwidth: boolean;
    icon: string;
    label: string;
    expandContent: boolean;
    buttonElement: HTMLElement;
    ripple: Promise<Ripple | null>;
    protected shouldRenderRipple: boolean;
    protected rippleHandlers: RippleHandlers;
    /** @soyTemplate */
    protected renderOverlay(): TemplateResult;
    /** @soyTemplate */
    protected renderRipple(): TemplateResult | string;
    focus(): void;
    blur(): void;
    /** @soyTemplate */
    protected getRenderClasses(): ClassInfo;
    /**
     * @soyTemplate
     * @soyAttributes buttonAttributes: #button
     * @soyClasses buttonClasses: #button
     */
    protected render(): TemplateResult;
    /** @soyTemplate */
    protected renderIcon(): TemplateResult;
    protected handleRippleActivate(evt?: Event): void;
    protected handleRippleDeactivate(): void;
    protected handleRippleMouseEnter(): void;
    protected handleRippleMouseLeave(): void;
    protected handleRippleFocus(): void;
    protected handleRippleBlur(): void;
}
