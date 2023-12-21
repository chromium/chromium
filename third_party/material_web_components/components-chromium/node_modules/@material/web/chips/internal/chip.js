/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import '../../focus/md-focus-ring.js';
import '../../ripple/ripple.js';
import { html, LitElement } from 'lit';
import { property } from 'lit/decorators.js';
import { classMap } from 'lit/directives/class-map.js';
import { requestUpdateOnAriaChange } from '../../internal/aria/delegate.js';
/**
 * A chip component.
 *
 * @fires update-focus {Event} Dispatched when `disabled` is toggled. --bubbles
 */
export class Chip extends LitElement {
    constructor() {
        super(...arguments);
        /**
         * Whether or not the chip is disabled.
         *
         * Disabled chips are not focusable, unless `always-focusable` is set.
         */
        this.disabled = false;
        /**
         * When true, allow disabled chips to be focused with arrow keys.
         *
         * Add this when a chip needs increased visibility when disabled. See
         * https://www.w3.org/WAI/ARIA/apg/practices/keyboard-interface/#kbd_disabled_controls
         * for more guidance on when this is needed.
         */
        this.alwaysFocusable = false;
        /**
         * The label of the chip.
         */
        this.label = '';
        /**
         * Only needed for SSR.
         *
         * Add this attribute when a chip has a `slot="icon"` to avoid a Flash Of
         * Unstyled Content.
         */
        this.hasIcon = false;
    }
    /**
     * Whether or not the primary ripple is disabled (defaults to `disabled`).
     * Some chip actions such as links cannot be disabled.
     */
    get rippleDisabled() {
        return this.disabled;
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
            'disabled': this.disabled,
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
      <span class="label">${this.label}</span>
      <span class="touch"></span>
    `;
    }
    handleIconChange(event) {
        const slot = event.target;
        this.hasIcon = slot.assignedElements({ flatten: true }).length > 0;
    }
}
(() => {
    requestUpdateOnAriaChange(Chip);
})();
/** @nocollapse */
Chip.shadowRootOptions = {
    ...LitElement.shadowRootOptions,
    delegatesFocus: true,
};
__decorate([
    property({ type: Boolean, reflect: true })
], Chip.prototype, "disabled", void 0);
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