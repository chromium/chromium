/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { LitElement } from 'lit';
import { property } from 'lit/decorators.js';
/**
 * A divider component.
 */
export class Divider extends LitElement {
    constructor() {
        super(...arguments);
        /**
         * Indents the divider with equal padding on both sides.
         */
        this.inset = false;
        /**
         * Indents the divider with padding on the leading side.
         */
        this.insetStart = false;
        /**
         * Indents the divider with padding on the trailing side.
         */
        this.insetEnd = false;
    }
}
__decorate([
    property({ type: Boolean, reflect: true })
], Divider.prototype, "inset", void 0);
__decorate([
    property({ type: Boolean, reflect: true, attribute: 'inset-start' })
], Divider.prototype, "insetStart", void 0);
__decorate([
    property({ type: Boolean, reflect: true, attribute: 'inset-end' })
], Divider.prototype, "insetEnd", void 0);
//# sourceMappingURL=divider.js.map