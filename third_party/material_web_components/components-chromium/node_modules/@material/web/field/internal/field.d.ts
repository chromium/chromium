/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement, PropertyValues, TemplateResult } from 'lit';
/**
 * A field component.
 */
export declare class Field extends LitElement {
    disabled: boolean;
    error: boolean;
    focused: boolean;
    label: string;
    noAsterisk: boolean;
    populated: boolean;
    required: boolean;
    resizable: boolean;
    supportingText: string;
    errorText: string;
    count: number;
    max: number;
    /**
     * Whether or not the field has leading content.
     */
    hasStart: boolean;
    /**
     * Whether or not the field has trailing content.
     */
    hasEnd: boolean;
    private readonly slottedAriaDescribedBy;
    private get counterText();
    private get supportingOrErrorText();
    private isAnimating;
    private labelAnimation?;
    /**
     * When set to true, the error text's `role="alert"` will be removed, then
     * re-added after an animation frame. This will re-announce an error message
     * to screen readers.
     */
    private refreshErrorAlert;
    private disableTransitions;
    private readonly floatingLabelEl;
    private readonly restingLabelEl;
    private readonly containerEl;
    /**
     * Re-announces the field's error supporting text to screen readers.
     *
     * Error text announces to screen readers anytime it is visible and changes.
     * Use the method to re-announce the message when the text has not changed,
     * but announcement is still needed (such as for `reportValidity()`).
     */
    reannounceError(): void;
    protected update(props: PropertyValues<Field>): void;
    protected render(): TemplateResult<1>;
    protected updated(changed: PropertyValues<Field>): void;
    protected renderBackground?(): TemplateResult;
    protected renderStateLayer?(): TemplateResult;
    protected renderIndicator?(): TemplateResult;
    protected renderOutline?(floatingLabel: unknown): TemplateResult;
    private renderSupportingText;
    private updateSlottedAriaDescribedBy;
    private renderLabel;
    private animateLabelIfNeeded;
    private getLabelKeyframes;
    getSurfacePositionClientRect(): DOMRect;
}
