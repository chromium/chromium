/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../elevation/elevation.js';
import { Chip } from './chip.js';
/**
 * An assist chip component.
 */
export declare class AssistChip extends Chip {
    elevated: boolean;
    href: string;
    target: '_blank' | '_parent' | '_self' | '_top' | '';
    protected get primaryId(): "link" | "button";
    protected get rippleDisabled(): boolean;
    protected getContainerClasses(): {
        disabled: boolean;
        elevated: boolean;
        link: boolean;
    };
    protected renderPrimaryAction(content: unknown): import("lit-html").TemplateResult<1>;
    protected renderOutline(): import("lit-html").TemplateResult<1>;
}
