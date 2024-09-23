/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import '../../elevation/elevation.js';
import '../../focus/md-focus-ring.js';
import '../../ripple/ripple.js';
import { html, LitElement, nothing } from 'lit';
import { property } from 'lit/decorators.js';
import { classMap } from 'lit/directives/class-map.js';
import { mixinDelegatesAria } from '../../internal/aria/delegate.js';
// Separate variable needed for closure.
const fabBaseClass = mixinDelegatesAria(LitElement);
// tslint:disable-next-line:enforce-comments-on-exported-symbols
export class SharedFab extends fabBaseClass {
    constructor() {
        super(...arguments);
        /**
         * The size of the FAB.
         *
         * NOTE: Branded FABs cannot be sized to `small`, and Extended FABs do not
         * have different sizes.
         */
        this.size = 'medium';
        /**
         * The text to display on the FAB.
         */
        this.label = '';
        /**
         * Lowers the FAB's elevation.
         */
        this.lowered = false;
    }
    render() {
        // Needed for closure conformance
        const { ariaLabel } = this;
        return html `
      <button
        class="fab ${classMap(this.getRenderClasses())}"
        aria-label=${ariaLabel || nothing}>
        <md-elevation part="elevation"></md-elevation>
        <md-focus-ring part="focus-ring"></md-focus-ring>
        <md-ripple class="ripple"></md-ripple>
        ${this.renderTouchTarget()} ${this.renderIcon()} ${this.renderLabel()}
      </button>
    `;
    }
    getRenderClasses() {
        const isExtended = !!this.label;
        return {
            'lowered': this.lowered,
            'small': this.size === 'small' && !isExtended,
            'large': this.size === 'large' && !isExtended,
            'extended': isExtended,
        };
    }
    renderTouchTarget() {
        return html `<div class="touch-target"></div>`;
    }
    renderLabel() {
        return this.label ? html `<span class="label">${this.label}</span>` : '';
    }
    renderIcon() {
        const { ariaLabel } = this;
        return html `<span class="icon">
      <slot
        name="icon"
        aria-hidden=${ariaLabel || this.label
            ? 'true'
            : nothing}>
        <span></span>
      </slot>
    </span>`;
    }
}
/** @nocollapse */
SharedFab.shadowRootOptions = {
    mode: 'open',
    delegatesFocus: true,
};
__decorate([
    property({ reflect: true })
], SharedFab.prototype, "size", void 0);
__decorate([
    property()
], SharedFab.prototype, "label", void 0);
__decorate([
    property({ type: Boolean })
], SharedFab.prototype, "lowered", void 0);
//# sourceMappingURL=shared.js.map