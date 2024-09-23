/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import '../../../focus/md-focus-ring.js';
import '../../../labs/item/item.js';
import '../../../ripple/ripple.js';
import { html, LitElement, nothing } from 'lit';
import { property, query } from 'lit/decorators.js';
import { classMap } from 'lit/directives/class-map.js';
import { literal, html as staticHtml } from 'lit/static-html.js';
import { mixinDelegatesAria } from '../../../internal/aria/delegate.js';
import { createRequestActivationEvent, } from '../list-navigation-helpers.js';
// Separate variable needed for closure.
const listItemBaseClass = mixinDelegatesAria(LitElement);
/**
 * @fires request-activation {Event} Requests the list to set `tabindex=0` on
 * the item and focus it. --bubbles --composed
 */
export class ListItemEl extends listItemBaseClass {
    constructor() {
        super(...arguments);
        /**
         * Disables the item and makes it non-selectable and non-interactive.
         */
        this.disabled = false;
        /**
         * Sets the behavior of the list item, defaults to "text". Change to "link" or
         * "button" for interactive items.
         */
        this.type = 'text';
        /**
         * READONLY. Sets the `md-list-item` attribute on the element.
         */
        this.isListItem = true;
        /**
         * Sets the underlying `HTMLAnchorElement`'s `href` resource attribute.
         */
        this.href = '';
        /**
         * Sets the underlying `HTMLAnchorElement`'s `target` attribute when `href` is
         * set.
         */
        this.target = '';
    }
    get isDisabled() {
        return this.disabled && this.type !== 'link';
    }
    willUpdate(changed) {
        if (this.href) {
            this.type = 'link';
        }
        super.willUpdate(changed);
    }
    render() {
        return this.renderListItem(html `
      <md-item>
        <div slot="container">
          ${this.renderRipple()} ${this.renderFocusRing()}
        </div>
        <slot name="start" slot="start"></slot>
        <slot name="end" slot="end"></slot>
        ${this.renderBody()}
      </md-item>
    `);
    }
    /**
     * Renders the root list item.
     *
     * @param content the child content of the list item.
     */
    renderListItem(content) {
        const isAnchor = this.type === 'link';
        let tag;
        switch (this.type) {
            case 'link':
                tag = literal `a`;
                break;
            case 'button':
                tag = literal `button`;
                break;
            default:
            case 'text':
                tag = literal `li`;
                break;
        }
        const isInteractive = this.type !== 'text';
        // TODO(b/265339866): announce "button"/"link" inside of a list item. Until
        // then all are "listitem" roles for correct announcement.
        const target = isAnchor && !!this.target ? this.target : nothing;
        return staticHtml `
      <${tag}
        id="item"
        tabindex="${this.isDisabled || !isInteractive ? -1 : 0}"
        ?disabled=${this.isDisabled}
        role="listitem"
        aria-selected=${this.ariaSelected || nothing}
        aria-checked=${this.ariaChecked || nothing}
        aria-expanded=${this.ariaExpanded || nothing}
        aria-haspopup=${this.ariaHasPopup || nothing}
        class="list-item ${classMap(this.getRenderClasses())}"
        href=${this.href || nothing}
        target=${target}
        @focus=${this.onFocus}
      >${content}</${tag}>
    `;
    }
    /**
     * Handles rendering of the ripple element.
     */
    renderRipple() {
        if (this.type === 'text') {
            return nothing;
        }
        return html ` <md-ripple
      part="ripple"
      for="item"
      ?disabled=${this.isDisabled}></md-ripple>`;
    }
    /**
     * Handles rendering of the focus ring.
     */
    renderFocusRing() {
        if (this.type === 'text') {
            return nothing;
        }
        return html ` <md-focus-ring
      @visibility-changed=${this.onFocusRingVisibilityChanged}
      part="focus-ring"
      for="item"
      inward></md-focus-ring>`;
    }
    onFocusRingVisibilityChanged(e) { }
    /**
     * Classes applied to the list item root.
     */
    getRenderClasses() {
        return { 'disabled': this.isDisabled };
    }
    /**
     * Handles rendering the headline and supporting text.
     */
    renderBody() {
        return html `
      <slot></slot>
      <slot name="overline" slot="overline"></slot>
      <slot name="headline" slot="headline"></slot>
      <slot name="supporting-text" slot="supporting-text"></slot>
      <slot
        name="trailing-supporting-text"
        slot="trailing-supporting-text"></slot>
    `;
    }
    onFocus() {
        if (this.tabIndex !== -1) {
            return;
        }
        // Handles the case where the user clicks on the element and then tabs.
        this.dispatchEvent(createRequestActivationEvent());
    }
    focus() {
        // TODO(b/300334509): needed for some cases where delegatesFocus doesn't
        // work programmatically like in FF and select-option
        this.listItemRoot?.focus();
    }
}
/** @nocollapse */
ListItemEl.shadowRootOptions = {
    ...LitElement.shadowRootOptions,
    delegatesFocus: true,
};
__decorate([
    property({ type: Boolean, reflect: true })
], ListItemEl.prototype, "disabled", void 0);
__decorate([
    property({ reflect: true })
], ListItemEl.prototype, "type", void 0);
__decorate([
    property({ type: Boolean, attribute: 'md-list-item', reflect: true })
], ListItemEl.prototype, "isListItem", void 0);
__decorate([
    property()
], ListItemEl.prototype, "href", void 0);
__decorate([
    property()
], ListItemEl.prototype, "target", void 0);
__decorate([
    query('.list-item')
], ListItemEl.prototype, "listItemRoot", void 0);
//# sourceMappingURL=list-item.js.map