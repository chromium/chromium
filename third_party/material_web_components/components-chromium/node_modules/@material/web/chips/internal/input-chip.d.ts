/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { MultiActionChip } from './multi-action-chip.js';
/**
 * An input chip component.
 *
 * @fires remove {Event} Dispatched when the remove button is clicked.
 */
export declare class InputChip extends MultiActionChip {
    avatar: boolean;
    href: string;
    target: '_blank' | '_parent' | '_self' | '_top' | '';
    removeOnly: boolean;
    selected: boolean;
    protected get primaryId(): "" | "link" | "button";
    protected get rippleDisabled(): boolean;
    protected get primaryAction(): HTMLElement;
    protected readonly trailingAction: HTMLElement | null;
    protected getContainerClasses(): {
        avatar: boolean;
        disabled: boolean;
        link: boolean;
        selected: boolean;
        'has-trailing': boolean;
    };
    protected renderPrimaryAction(content: unknown): import("lit-html").TemplateResult<1>;
    protected renderTrailingAction(focusListener: EventListener): import("lit-html").TemplateResult<1>;
}
