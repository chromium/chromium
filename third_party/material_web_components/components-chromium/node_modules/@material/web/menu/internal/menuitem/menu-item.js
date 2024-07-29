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
import { property, query, queryAssignedElements, queryAssignedNodes, } from 'lit/decorators.js';
import { classMap } from 'lit/directives/class-map.js';
import { literal, html as staticHtml } from 'lit/static-html.js';
import { mixinDelegatesAria } from '../../../internal/aria/delegate.js';
import { MenuItemController, } from '../controllers/menuItemController.js';
// Separate variable needed for closure.
const menuItemBaseClass = mixinDelegatesAria(LitElement);
/**
 * @fires close-menu {CustomEvent<{initiator: SelectOption, reason: Reason, itemPath: SelectOption[]}>}
 * Closes the encapsulating menu on closable interaction. --bubbles --composed
 */
export class MenuItemEl extends menuItemBaseClass {
    constructor() {
        super(...arguments);
        /**
         * Disables the item and makes it non-selectable and non-interactive.
         */
        this.disabled = false;
        /**
         * Sets the behavior and role of the menu item, defaults to "menuitem".
         */
        this.type = 'menuitem';
        /**
         * Sets the underlying `HTMLAnchorElement`'s `href` resource attribute.
         */
        this.href = '';
        /**
         * Sets the underlying `HTMLAnchorElement`'s `target` attribute when `href` is
         * set.
         */
        this.target = '';
        /**
         * Keeps the menu open if clicked or keyboard selected.
         */
        this.keepOpen = false;
        /**
         * Sets the item in the selected visual state when a submenu is opened.
         */
        this.selected = false;
        this.menuItemController = new MenuItemController(this, {
            getHeadlineElements: () => {
                return this.headlineElements;
            },
            getSupportingTextElements: () => {
                return this.supportingTextElements;
            },
            getDefaultElements: () => {
                return this.defaultElements;
            },
            getInteractiveElement: () => this.listItemRoot,
        });
    }
    /**
     * The text that is selectable via typeahead. If not set, defaults to the
     * innerText of the item slotted into the `"headline"` slot.
     */
    get typeaheadText() {
        return this.menuItemController.typeaheadText;
    }
    set typeaheadText(text) {
        this.menuItemController.setTypeaheadText(text);
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
        switch (this.menuItemController.tagName) {
            case 'a':
                tag = literal `a`;
                break;
            case 'button':
                tag = literal `button`;
                break;
            default:
            case 'li':
                tag = literal `li`;
                break;
        }
        // TODO(b/265339866): announce "button"/"link" inside of a list item. Until
        // then all are "menuitem" roles for correct announcement.
        const target = isAnchor && !!this.target ? this.target : nothing;
        return staticHtml `
      <${tag}
        id="item"
        tabindex=${this.disabled && !isAnchor ? -1 : 0}
        role=${this.menuItemController.role}
        aria-label=${this.ariaLabel || nothing}
        aria-selected=${this.ariaSelected || nothing}
        aria-checked=${this.ariaChecked || nothing}
        aria-expanded=${this.ariaExpanded || nothing}
        aria-haspopup=${this.ariaHasPopup || nothing}
        class="list-item ${classMap(this.getRenderClasses())}"
        href=${this.href || nothing}
        target=${target}
        @click=${this.menuItemController.onClick}
        @keydown=${this.menuItemController.onKeydown}
      >${content}</${tag}>
    `;
    }
    /**
     * Handles rendering of the ripple element.
     */
    renderRipple() {
        return html ` <md-ripple
      part="ripple"
      for="item"
      ?disabled=${this.disabled}></md-ripple>`;
    }
    /**
     * Handles rendering of the focus ring.
     */
    renderFocusRing() {
        return html ` <md-focus-ring
      part="focus-ring"
      for="item"
      inward></md-focus-ring>`;
    }
    /**
     * Classes applied to the list item root.
     */
    getRenderClasses() {
        return {
            'disabled': this.disabled,
            'selected': this.selected,
        };
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
    focus() {
        // TODO(b/300334509): needed for some cases where delegatesFocus doesn't
        // work programmatically like in FF and select-option
        this.listItemRoot?.focus();
    }
}
/** @nocollapse */
MenuItemEl.shadowRootOptions = {
    ...LitElement.shadowRootOptions,
    delegatesFocus: true,
};
__decorate([
    property({ type: Boolean, reflect: true })
], MenuItemEl.prototype, "disabled", void 0);
__decorate([
    property()
], MenuItemEl.prototype, "type", void 0);
__decorate([
    property()
], MenuItemEl.prototype, "href", void 0);
__decorate([
    property()
], MenuItemEl.prototype, "target", void 0);
__decorate([
    property({ type: Boolean, attribute: 'keep-open' })
], MenuItemEl.prototype, "keepOpen", void 0);
__decorate([
    property({ type: Boolean })
], MenuItemEl.prototype, "selected", void 0);
__decorate([
    query('.list-item')
], MenuItemEl.prototype, "listItemRoot", void 0);
__decorate([
    queryAssignedElements({ slot: 'headline' })
], MenuItemEl.prototype, "headlineElements", void 0);
__decorate([
    queryAssignedElements({ slot: 'supporting-text' })
], MenuItemEl.prototype, "supportingTextElements", void 0);
__decorate([
    queryAssignedNodes({ slot: '' })
], MenuItemEl.prototype, "defaultElements", void 0);
__decorate([
    property({ attribute: 'typeahead-text' })
], MenuItemEl.prototype, "typeaheadText", null);
//# sourceMappingURL=menu-item.js.map