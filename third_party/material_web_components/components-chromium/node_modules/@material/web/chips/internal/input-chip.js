/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { html, nothing } from 'lit';
import { property, query } from 'lit/decorators.js';
import { MultiActionChip } from './multi-action-chip.js';
import { renderRemoveButton } from './trailing-icons.js';
/**
 * An input chip component.
 *
 * @fires remove {Event} Dispatched when the remove button is clicked.
 */
export class InputChip extends MultiActionChip {
    constructor() {
        super(...arguments);
        this.avatar = false;
        this.href = '';
        this.target = '';
        this.removeOnly = false;
        this.selected = false;
    }
    get primaryId() {
        if (this.href) {
            return 'link';
        }
        if (this.removeOnly) {
            return '';
        }
        return 'button';
    }
    get rippleDisabled() {
        // Link chips cannot be disabled
        return !this.href && (this.disabled || this.softDisabled);
    }
    get primaryAction() {
        // Don't use @query() since a remove-only input chip still has a span that
        // has "primary action" classes.
        if (this.removeOnly) {
            return null;
        }
        return this.renderRoot.querySelector('.primary.action');
    }
    getContainerClasses() {
        return {
            ...super.getContainerClasses(),
            avatar: this.avatar,
            // Link chips cannot be disabled
            disabled: !this.href && (this.disabled || this.softDisabled),
            link: !!this.href,
            selected: this.selected,
            'has-trailing': true,
        };
    }
    renderPrimaryAction(content) {
        const { ariaLabel } = this;
        if (this.href) {
            return html `
        <a
          class="primary action"
          id="link"
          aria-label=${ariaLabel || nothing}
          href=${this.href}
          target=${this.target || nothing}
          >${content}</a
        >
      `;
        }
        if (this.removeOnly) {
            return html `
        <span class="primary action" aria-label=${ariaLabel || nothing}>
          ${content}
        </span>
      `;
        }
        return html `
      <button
        class="primary action"
        id="button"
        aria-label=${ariaLabel || nothing}
        aria-disabled=${this.softDisabled || nothing}
        ?disabled=${this.disabled && !this.alwaysFocusable}
        type="button"
        >${content}</button
      >
    `;
    }
    renderTrailingAction(focusListener) {
        return renderRemoveButton({
            focusListener,
            ariaLabel: this.ariaLabelRemove,
            disabled: !this.href && (this.disabled || this.softDisabled),
            tabbable: this.removeOnly,
        });
    }
}
__decorate([
    property({ type: Boolean })
], InputChip.prototype, "avatar", void 0);
__decorate([
    property()
], InputChip.prototype, "href", void 0);
__decorate([
    property()
], InputChip.prototype, "target", void 0);
__decorate([
    property({ type: Boolean, attribute: 'remove-only' })
], InputChip.prototype, "removeOnly", void 0);
__decorate([
    property({ type: Boolean, reflect: true })
], InputChip.prototype, "selected", void 0);
__decorate([
    query('.trailing.action')
], InputChip.prototype, "trailingAction", void 0);
//# sourceMappingURL=input-chip.js.map