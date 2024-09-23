/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import '../../focus/md-focus-ring.js';
import '../../ripple/ripple.js';
import { html, isServer, LitElement } from 'lit';
import { property } from 'lit/decorators.js';
import { classMap } from 'lit/directives/class-map.js';
import { mixinDelegatesAria } from '../../internal/aria/delegate.js';
// Separate variable needed for closure.
const chipBaseClass = mixinDelegatesAria(LitElement);
/**
 * A chip component.
 *
 * @fires update-focus {Event} Dispatched when `disabled` is toggled. --bubbles
 */
export class Chip extends chipBaseClass {
    /**
     * Whether or not the primary ripple is disabled (defaults to `disabled`).
     * Some chip actions such as links cannot be disabled.
     */
    get rippleDisabled() {
        return this.disabled || this.softDisabled;
    }
    constructor() {
        super();
        /**
         * Whether or not the chip is disabled.
         *
         * Disabled chips are not focusable, unless `always-focusable` is set.
         */
        this.disabled = false;
        /**
         * Whether or not the chip is "soft-disabled" (disabled but still
         * focusable).
         *
         * Use this when a chip needs increased visibility when disabled. See
         * https://www.w3.org/WAI/ARIA/apg/practices/keyboard-interface/#kbd_disabled_controls
         * for more guidance on when this is needed.
         */
        this.softDisabled = false;
        /**
         * When true, allow disabled chips to be focused with arrow keys.
         *
         * Add this when a chip needs increased visibility when disabled. See
         * https://www.w3.org/WAI/ARIA/apg/practices/keyboard-interface/#kbd_disabled_controls
         * for more guidance on when this is needed.
         *
         * @deprecated Use `softDisabled` instead of `alwaysFocusable` + `disabled`.
         */
        this.alwaysFocusable = false;
        // TODO(b/350810013): remove the label property.
        /**
         * The label of the chip.
         *
         * @deprecated Set text as content of the chip instead.
         */
        this.label = '';
        /**
         * Only needed for SSR.
         *
         * Add this attribute when a chip has a `slot="icon"` to avoid a Flash Of
         * Unstyled Content.
         */
        this.hasIcon = false;
        if (!isServer) {
            this.addEventListener('click', this.handleClick.bind(this));
        }
    }
    focus(options) {
        if (this.disabled && !this.alwaysFocusable) {
            return;
        }
        super.focus(options);
    }
    render() {
        return html `
      <div class="container ${classMap(this.getContainerClasses())}">
        ${this.renderContainerContent()}
      </div>
    `;
    }
    updated(changed) {
        if (changed.has('disabled') && changed.get('disabled') !== undefined) {
            this.dispatchEvent(new Event('update-focus', { bubbles: true }));
        }
    }
    getContainerClasses() {
        return {
            'disabled': this.disabled || this.softDisabled,
            'has-icon': this.hasIcon,
        };
    }
    renderContainerContent() {
        return html `
      ${this.renderOutline()}
      <md-focus-ring part="focus-ring" for=${this.primaryId}></md-focus-ring>
      <md-ripple
        for=${this.primaryId}
        ?disabled=${this.rippleDisabled}></md-ripple>
      ${this.renderPrimaryAction(this.renderPrimaryContent())}
    `;
    }
    renderOutline() {
        return html `<span class="outline"></span>`;
    }
    renderLeadingIcon() {
        return html `<slot name="icon" @slotchange=${this.handleIconChange}></slot>`;
    }
    renderPrimaryContent() {
        return html `
      <span class="leading icon" aria-hidden="true">
        ${this.renderLeadingIcon()}
      </span>
      <span class="label">
        <span class="label-text" id="label">
          ${this.label ? this.label : html `<slot></slot>`}
        </span>
      </span>
      <span class="touch"></span>
    `;
    }
    handleIconChange(event) {
        const slot = event.target;
        this.hasIcon = slot.assignedElements({ flatten: true }).length > 0;
    }
    handleClick(event) {
        // If the chip is soft-disabled or disabled + always-focusable, we need to
        // explicitly prevent the click from propagating to other event listeners
        // as well as prevent the default action.
        if (this.softDisabled || (this.disabled && this.alwaysFocusable)) {
            event.stopImmediatePropagation();
            event.preventDefault();
            return;
        }
    }
}
/** @nocollapse */
Chip.shadowRootOptions = {
    ...LitElement.shadowRootOptions,
    delegatesFocus: true,
};
__decorate([
    property({ type: Boolean, reflect: true })
], Chip.prototype, "disabled", void 0);
__decorate([
    property({ type: Boolean, attribute: 'soft-disabled', reflect: true })
], Chip.prototype, "softDisabled", void 0);
__decorate([
    property({ type: Boolean, attribute: 'always-focusable' })
], Chip.prototype, "alwaysFocusable", void 0);
__decorate([
    property()
], Chip.prototype, "label", void 0);
__decorate([
    property({ type: Boolean, reflect: true, attribute: 'has-icon' })
], Chip.prototype, "hasIcon", void 0);
//# sourceMappingURL=chip.js.map