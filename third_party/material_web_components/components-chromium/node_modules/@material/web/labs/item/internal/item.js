/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { html, LitElement } from 'lit';
import { property, queryAll } from 'lit/decorators.js';
/**
 * An item layout component.
 */
export class Item extends LitElement {
    constructor() {
        super(...arguments);
        /**
         * Only needed for SSR.
         *
         * Add this attribute when an item has two lines to avoid a Flash Of Unstyled
         * Content. This attribute is not needed for single line items or items with
         * three or more lines.
         */
        this.multiline = false;
    }
    render() {
        return html `
      <slot name="container"></slot>
      <slot class="non-text" name="start"></slot>
      <div class="text">
        <slot name="overline" @slotchange=${this.handleTextSlotChange}></slot>
        <slot
          class="default-slot"
          @slotchange=${this.handleTextSlotChange}></slot>
        <slot name="headline" @slotchange=${this.handleTextSlotChange}></slot>
        <slot
          name="supporting-text"
          @slotchange=${this.handleTextSlotChange}></slot>
      </div>
      <slot class="non-text" name="trailing-supporting-text"></slot>
      <slot class="non-text" name="end"></slot>
    `;
    }
    handleTextSlotChange() {
        // Check if there's more than one text slot with content. If so, the item is
        // multiline, which has a different min-height than single line items.
        let isMultiline = false;
        let slotsWithContent = 0;
        for (const slot of this.textSlots) {
            if (slotHasContent(slot)) {
                slotsWithContent += 1;
            }
            if (slotsWithContent > 1) {
                isMultiline = true;
                break;
            }
        }
        this.multiline = isMultiline;
    }
}
__decorate([
    property({ type: Boolean, reflect: true })
], Item.prototype, "multiline", void 0);
__decorate([
    queryAll('.text slot')
], Item.prototype, "textSlots", void 0);
function slotHasContent(slot) {
    for (const node of slot.assignedNodes({ flatten: true })) {
        // Assume there's content if there's an element slotted in
        const isElement = node.nodeType === Node.ELEMENT_NODE;
        // If there's only text nodes for the default slot, check if there's
        // non-whitespace.
        const isTextWithContent = node.nodeType === Node.TEXT_NODE && node.textContent?.match(/\S/);
        if (isElement || isTextWithContent) {
            return true;
        }
    }
    return false;
}
//# sourceMappingURL=item.js.map