/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement, TemplateResult } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
import { SegmentedButton } from '../../segmentedbutton/lib/segmented-button.js';
/**
 * SegmentedButtonSet is the parent component for two or more
 * `SegmentedButton` components. **Only** `SegmentedButton` components may be
 * used as children.
 * @soyCompatible
 */
export declare class SegmentedButtonSet extends LitElement {
    multiselect: boolean;
    /** @soyPrefixAttribute */
    ariaLabel: string;
    buttons: SegmentedButton[];
    getButtonDisabled(index: number): boolean;
    setButtonDisabled(index: number, disabled: boolean): void;
    getButtonSelected(index: number): boolean;
    setButtonSelected(index: number, selected: boolean): void;
    private handleSegmentedButtonInteraction;
    private toggleSelection;
    private indexOutOfBounds;
    private emitSelectionEvent;
    /** @soyTemplate */
    render(): TemplateResult;
    /** @soyTemplate */
    protected getRenderClasses(): ClassInfo;
}
