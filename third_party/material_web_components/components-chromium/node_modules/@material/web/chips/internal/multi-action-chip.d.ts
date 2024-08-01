/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Chip } from './chip.js';
/**
 * A chip component with multiple actions.
 */
export declare abstract class MultiActionChip extends Chip {
    get ariaLabelRemove(): string | null;
    set ariaLabelRemove(ariaLabel: string | null);
    protected abstract readonly primaryAction: HTMLElement | null;
    protected abstract readonly trailingAction: HTMLElement | null;
    constructor();
    focus(options?: FocusOptions & {
        trailing?: boolean;
    }): void;
    protected renderContainerContent(): import("lit-html").TemplateResult<1>;
    protected abstract renderTrailingAction(focusListener: EventListener): unknown;
    private handleKeyDown;
    private handleTrailingActionFocus;
}
