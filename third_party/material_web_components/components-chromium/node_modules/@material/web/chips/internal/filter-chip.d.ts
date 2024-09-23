/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../elevation/elevation.js';
import { nothing } from 'lit';
import { MultiActionChip } from './multi-action-chip.js';
/**
 * A filter chip component.
 *
 * @fires remove {Event} Dispatched when the remove button is clicked.
 */
export declare class FilterChip extends MultiActionChip {
    elevated: boolean;
    removable: boolean;
    selected: boolean;
    /**
     * Only needed for SSR.
     *
     * Add this attribute when a filter chip has a `slot="selected-icon"` to avoid
     * a Flash Of Unstyled Content.
     */
    hasSelectedIcon: boolean;
    protected get primaryId(): string;
    protected readonly primaryAction: HTMLElement | null;
    protected readonly trailingAction: HTMLElement | null;
    protected getContainerClasses(): {
        elevated: boolean;
        selected: boolean;
        'has-trailing': boolean;
        'has-icon': boolean;
    };
    protected renderPrimaryAction(content: unknown): import("lit-html").TemplateResult<1>;
    protected renderLeadingIcon(): import("lit-html").TemplateResult;
    protected renderTrailingAction(focusListener: EventListener): import("lit-html").TemplateResult<1> | typeof nothing;
    protected renderOutline(): import("lit-html").TemplateResult<1>;
    private handleClickOnChild;
}
