/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement, PropertyValues, TemplateResult } from 'lit';
import { SurfacePositionTarget } from '../../menu/lib/surfacePositionController.js';
/**
 * A field component.
 */
export declare class Field extends LitElement implements SurfacePositionTarget {
    disabled: boolean;
    error: boolean;
    focused: boolean;
    label?: string;
    populated: boolean;
    resizable: boolean;
    required: boolean;
    /**
     * Whether or not the field has leading content.
     */
    hasStart: boolean;
    /**
     * Whether or not the field has trailing content.
     */
    hasEnd: boolean;
    private isAnimating;
    private labelAnimation?;
    private readonly floatingLabelEl;
    private readonly restingLabelEl;
    private readonly containerEl;
    protected update(props: PropertyValues<Field>): void;
    protected render(): TemplateResult<1>;
    protected renderBackground?(): TemplateResult;
    protected renderIndicator?(): TemplateResult;
    protected renderOutline?(floatingLabel: TemplateResult): TemplateResult;
    private renderLabel;
    private animateLabelIfNeeded;
    private getLabelKeyframes;
    getSurfacePositionClientRect(): DOMRect;
}
