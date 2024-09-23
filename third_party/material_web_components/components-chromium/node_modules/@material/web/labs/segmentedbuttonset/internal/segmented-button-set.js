/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { html, LitElement, nothing } from 'lit';
import { property, queryAssignedElements } from 'lit/decorators.js';
import { mixinDelegatesAria } from '../../../internal/aria/delegate.js';
// Separate variable needed for closure.
const segmentedButtonSetBaseClass = mixinDelegatesAria(LitElement);
/**
 * SegmentedButtonSet is the parent component for two or more
 * `SegmentedButton` components. **Only** `SegmentedButton` components may be
 * used as children.
 *
 * @fires segmented-button-set-selection {CustomEvent<{button: SegmentedButton, selected: boolean, index: number}>}
 * Dispatched when a button is selected programattically with the
 * `setButtonSelected` or the `toggleSelection` methods as well as on user
 * interaction. --bubbles --composed
 */
export class SegmentedButtonSet extends segmentedButtonSetBaseClass {
    constructor() {
        super(...arguments);
        this.multiselect = false;
    }
    getButtonDisabled(index) {
        if (this.indexOutOfBounds(index))
            return false;
        return this.buttons[index].disabled;
    }
    setButtonDisabled(index, disabled) {
        if (this.indexOutOfBounds(index))
            return;
        this.buttons[index].disabled = disabled;
    }
    getButtonSelected(index) {
        if (this.indexOutOfBounds(index))
            return false;
        return this.buttons[index].selected;
    }
    setButtonSelected(index, selected) {
        // Ignore out-of-index values.
        if (this.indexOutOfBounds(index))
            return;
        // Ignore disabled buttons.
        if (this.getButtonDisabled(index))
            return;
        if (this.multiselect) {
            this.buttons[index].selected = selected;
            this.emitSelectionEvent(index);
            return;
        }
        // Single-select segmented buttons are not unselectable.
        if (!selected)
            return;
        this.buttons[index].selected = true;
        this.emitSelectionEvent(index);
        // Deselect all other buttons for single-select.
        for (let i = 0; i < this.buttons.length; i++) {
            if (i === index)
                continue;
            this.buttons[i].selected = false;
        }
    }
    handleSegmentedButtonInteraction(event) {
        const index = this.buttons.indexOf(event.target);
        this.toggleSelection(index);
    }
    toggleSelection(index) {
        if (this.indexOutOfBounds(index))
            return;
        this.setButtonSelected(index, !this.buttons[index].selected);
    }
    indexOutOfBounds(index) {
        return index < 0 || index >= this.buttons.length;
    }
    emitSelectionEvent(index) {
        this.dispatchEvent(new CustomEvent('segmented-button-set-selection', {
            detail: {
                button: this.buttons[index],
                selected: this.buttons[index].selected,
                index,
            },
            bubbles: true,
            composed: true,
        }));
    }
    render() {
        // Needed for closure conformance
        const { ariaLabel } = this;
        return html `
      <span
        role="group"
        @segmented-button-interaction="${this.handleSegmentedButtonInteraction}"
        aria-label=${ariaLabel || nothing}
        class="md3-segmented-button-set">
        <slot></slot>
      </span>
    `;
    }
    getRenderClasses() {
        return {};
    }
}
__decorate([
    property({ type: Boolean })
], SegmentedButtonSet.prototype, "multiselect", void 0);
__decorate([
    queryAssignedElements({ flatten: true })
], SegmentedButtonSet.prototype, "buttons", void 0);
//# sourceMappingURL=segmented-button-set.js.map