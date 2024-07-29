/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../../focus/md-focus-ring.js';
import '../../../ripple/ripple.js';
import { LitElement, nothing, PropertyValues, TemplateResult } from 'lit';
declare const segmentedButtonBaseClass: import("../../behaviors/mixin.js").MixinReturn<typeof LitElement>;
/**
 * SegmentedButton is a web component implementation of the Material Design
 * segmented button component. It is intended **only** for use as a child of a
 * `SementedButtonSet` component. It is **not** intended for use in any other
 * context.
 *
 * @fires segmented-button-interaction {Event} Dispatched whenever a button is
 * clicked. --bubbles --composed
 */
export declare class SegmentedButton extends segmentedButtonBaseClass {
    disabled: boolean;
    selected: boolean;
    label: string;
    noCheckmark: boolean;
    hasIcon: boolean;
    private animState;
    private readonly iconElement;
    protected update(props: PropertyValues<SegmentedButton>): void;
    private nextAnimationState;
    private handleClick;
    protected render(): TemplateResult<1>;
    protected getRenderClasses(): {
        'md3-segmented-button--selected': boolean;
        'md3-segmented-button--unselected': boolean;
        'md3-segmented-button--with-label': boolean;
        'md3-segmented-button--without-label': boolean;
        'md3-segmented-button--with-icon': boolean;
        'md3-segmented-button--with-checkmark': boolean;
        'md3-segmented-button--without-checkmark': boolean;
        'md3-segmented-button--selecting': boolean;
        'md3-segmented-button--deselecting': boolean;
    };
    protected renderOutline(): TemplateResult | typeof nothing;
    private renderLeading;
    private renderLeadingWithoutLabel;
    private renderLeadingWithLabel;
    private renderLabel;
    private renderTouchTarget;
}
export {};
