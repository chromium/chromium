/**
 * @license
 * Copyright 2018 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { BaseElement } from '@material/mwc-base/base-element.js';
import { RippleInterface } from '@material/mwc-base/utils.js';
import { MDCRippleAdapter } from '@material/ripple/adapter.js';
import MDCRippleFoundation from '@material/ripple/foundation.js';
import { PropertyValues, TemplateResult } from 'lit';
/** @soyCompatible */
export declare class RippleBase extends BaseElement implements RippleInterface {
    mdcRoot: HTMLElement;
    primary: boolean;
    accent: boolean;
    unbounded: boolean;
    disabled: boolean;
    activated: boolean;
    selected: boolean;
    internalUseStateLayerCustomProperties: boolean;
    protected hovering: boolean;
    protected bgFocused: boolean;
    protected fgActivation: boolean;
    protected fgDeactivation: boolean;
    protected fgScale: string;
    protected fgSize: string;
    protected translateStart: string;
    protected translateEnd: string;
    protected leftPos: string;
    protected topPos: string;
    protected mdcFoundationClass: typeof MDCRippleFoundation;
    protected mdcFoundation: MDCRippleFoundation;
    get isActive(): any;
    createAdapter(): MDCRippleAdapter;
    startPress(ev?: Event): void;
    endPress(): void;
    startFocus(): void;
    endFocus(): void;
    startHover(): void;
    endHover(): void;
    /**
     * Wait for the MDCFoundation to be created by `firstUpdated`
     */
    protected waitForFoundation(fn: () => void): void;
    protected update(changedProperties: PropertyValues<this>): void;
    /** @soyTemplate */
    protected render(): TemplateResult;
}
