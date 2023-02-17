/**
 * @license
 * Copyright 2018 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../focus/focus-ring.js';
import '../../icon/icon.js';
import '../../ripple/ripple.js';
import { LitElement, TemplateResult } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
import { MdRipple } from '../../ripple/ripple.js';
import { ARIAHasPopup } from '../../types/aria.js';
export declare class IconButton extends LitElement {
    /**
     * Disables the icon button and makes it non-interactive.
     */
    disabled: boolean;
    /**
     * Flips the icon if it is in an RTL context at startup.
     */
    flipIconInRtl: boolean;
    protected flipIcon: boolean;
    ariaLabel: string;
    ariaHasPopup: ARIAHasPopup;
    protected buttonElement: HTMLElement;
    protected ripple: Promise<MdRipple | null>;
    protected showFocusRing: boolean;
    protected showRipple: boolean;
    protected getRipple: () => Promise<MdRipple>;
    protected readonly renderRipple: () => TemplateResult<1>;
    protected render(): TemplateResult;
    protected getRenderClasses(): ClassInfo;
    protected renderIcon(): TemplateResult;
    protected renderTouchTarget(): TemplateResult;
    protected renderFocusRing(): TemplateResult;
    connectedCallback(): void;
    handlePointerDown(): void;
    protected handleFocus(): void;
    protected handleBlur(): void;
}
