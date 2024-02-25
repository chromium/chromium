/**
 * @license
 * Copyright 2018 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../focus/md-focus-ring.js';
import '../../ripple/ripple.js';
import { LitElement } from 'lit';
import { FormSubmitter, FormSubmitterType } from '../../internal/controller/form-submitter.js';
type LinkTarget = '_blank' | '_parent' | '_self' | '_top';
declare const iconButtonBaseClass: import("../../labs/behaviors/mixin.js").MixinReturn<typeof LitElement, import("../../labs/behaviors/element-internals.js").WithElementInternals>;
/**
 * A button for rendering icons.
 *
 * @fires input {InputEvent} Dispatched when a toggle button toggles --bubbles
 * --composed
 * @fires change {Event} Dispatched when a toggle button toggles --bubbles
 */
export declare class IconButton extends iconButtonBaseClass implements FormSubmitter {
    /** @nocollapse */
    static readonly formAssociated = true;
    /** @nocollapse */
    static shadowRootOptions: ShadowRootInit;
    /**
     * Disables the icon button and makes it non-interactive.
     */
    disabled: boolean;
    /**
     * Flips the icon if it is in an RTL context at startup.
     */
    flipIconInRtl: boolean;
    /**
     * Sets the underlying `HTMLAnchorElement`'s `href` resource attribute.
     */
    href: string;
    /**
     * Sets the underlying `HTMLAnchorElement`'s `target` attribute.
     */
    target: LinkTarget | '';
    /**
     * The `aria-label` of the button when the button is toggleable and selected.
     */
    ariaLabelSelected: string;
    /**
     * When true, the button will toggle between selected and unselected
     * states
     */
    toggle: boolean;
    /**
     * Sets the selected state. When false, displays the default icon. When true,
     * displays the selected icon, or the default icon If no `slot="selected"`
     * icon is provided.
     */
    selected: boolean;
    type: FormSubmitterType;
    value: string;
    get name(): string;
    set name(name: string);
    /**
     * The associated form element with which this element's value will submit.
     */
    get form(): HTMLFormElement;
    /**
     * The labels this element is associated with.
     */
    get labels(): NodeList;
    private flipIcon;
    /**
     * Link buttons cannot be disabled.
     */
    protected willUpdate(): void;
    protected render(): import("lit-html").TemplateResult;
    private renderLink;
    protected getRenderClasses(): {
        'flip-icon': boolean;
        selected: boolean;
    };
    private renderIcon;
    private renderSelectedIcon;
    private renderTouchTarget;
    private renderFocusRing;
    private renderRipple;
    connectedCallback(): void;
    private handleClick;
}
export {};
