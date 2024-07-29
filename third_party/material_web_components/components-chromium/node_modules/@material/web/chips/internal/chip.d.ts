/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../focus/md-focus-ring.js';
import '../../ripple/ripple.js';
import { LitElement, PropertyValues, TemplateResult } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
declare const chipBaseClass: import("../../labs/behaviors/mixin.js").MixinReturn<typeof LitElement>;
/**
 * A chip component.
 *
 * @fires update-focus {Event} Dispatched when `disabled` is toggled. --bubbles
 */
export declare abstract class Chip extends chipBaseClass {
    /** @nocollapse */
    static shadowRootOptions: {
        delegatesFocus: boolean;
        mode: ShadowRootMode;
        slotAssignment?: SlotAssignmentMode;
    };
    /**
     * Whether or not the chip is disabled.
     *
     * Disabled chips are not focusable, unless `always-focusable` is set.
     */
    disabled: boolean;
    /**
     * Whether or not the chip is "soft-disabled" (disabled but still
     * focusable).
     *
     * Use this when a chip needs increased visibility when disabled. See
     * https://www.w3.org/WAI/ARIA/apg/practices/keyboard-interface/#kbd_disabled_controls
     * for more guidance on when this is needed.
     */
    softDisabled: boolean;
    /**
     * When true, allow disabled chips to be focused with arrow keys.
     *
     * Add this when a chip needs increased visibility when disabled. See
     * https://www.w3.org/WAI/ARIA/apg/practices/keyboard-interface/#kbd_disabled_controls
     * for more guidance on when this is needed.
     *
     * @deprecated Use `softDisabled` instead of `alwaysFocusable` + `disabled`.
     */
    alwaysFocusable: boolean;
    /**
     * The label of the chip.
     *
     * @deprecated Set text as content of the chip instead.
     */
    label: string;
    /**
     * Only needed for SSR.
     *
     * Add this attribute when a chip has a `slot="icon"` to avoid a Flash Of
     * Unstyled Content.
     */
    hasIcon: boolean;
    /**
     * The `id` of the action the primary focus ring and ripple are for.
     * TODO(b/310046938): use the same id for both elements
     */
    protected abstract readonly primaryId: string;
    /**
     * Whether or not the primary ripple is disabled (defaults to `disabled`).
     * Some chip actions such as links cannot be disabled.
     */
    protected get rippleDisabled(): boolean;
    constructor();
    focus(options?: FocusOptions): void;
    protected render(): TemplateResult<1>;
    protected updated(changed: PropertyValues<Chip>): void;
    protected getContainerClasses(): ClassInfo;
    protected renderContainerContent(): TemplateResult<1>;
    protected renderOutline(): TemplateResult<1>;
    protected renderLeadingIcon(): TemplateResult;
    protected abstract renderPrimaryAction(content: unknown): unknown;
    private renderPrimaryContent;
    private handleIconChange;
    private handleClick;
}
export {};
