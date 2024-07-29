/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement } from 'lit';
import { SegmentedButton } from '../../segmentedbutton/internal/segmented-button.js';
declare const segmentedButtonSetBaseClass: import("../../behaviors/mixin.js").MixinReturn<typeof LitElement>;
/**
 * SegmentedButtonSet is the parent component for two or more
 * `SegmentedButton` components. **Only** `SegmentedButton` components may be
 * used as children.
 *
 * @fires segmented-button-set-selection {CustomEvent<{button: SegmentedButton, selected: boolean, index: number}>}
 * Dispatched when a button is selected programattically with the
 * `setButtonSelected` or the `toggleSelection` methods as well as on user
 * interaction. --bubbles --composed
 */
export declare class SegmentedButtonSet extends segmentedButtonSetBaseClass {
    multiselect: boolean;
    buttons: SegmentedButton[];
    getButtonDisabled(index: number): boolean;
    setButtonDisabled(index: number, disabled: boolean): void;
    getButtonSelected(index: number): boolean;
    setButtonSelected(index: number, selected: boolean): void;
    private handleSegmentedButtonInteraction;
    private toggleSelection;
    private indexOutOfBounds;
    private emitSelectionEvent;
    protected render(): import("lit-html").TemplateResult<1>;
    protected getRenderClasses(): {};
}
export {};
