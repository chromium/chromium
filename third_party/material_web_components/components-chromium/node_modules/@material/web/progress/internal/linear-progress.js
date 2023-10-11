/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { html } from 'lit';
import { property } from 'lit/decorators.js';
import { styleMap } from 'lit/directives/style-map.js';
import { Progress } from './progress.js';
/**
 * A linear progress component.
 */
export class LinearProgress extends Progress {
    constructor() {
        super(...arguments);
        /**
         * Buffer amount to display, a fraction between 0 and 1.
         */
        this.buffer = 1;
    }
    // Note, the indeterminate animation is rendered with transform %'s
    // Previously, this was optimized to use px calculated with the resizeObserver
    // due to a now fixed Chrome bug: crbug.com/389359.
    renderIndicator() {
        const progressStyles = {
            transform: `scaleX(${(this.indeterminate ? 1 : this.value / this.max) * 100}%)`
        };
        const dotStyles = {
            transform: `scaleX(${(this.indeterminate ? 1 : this.buffer / this.max) * 100}%)`
        };
        return html `
      <div class="dots"></div>
      <div class="inactive-track" style=${styleMap(dotStyles)}></div>
      <div class="bar primary-bar" style=${styleMap(progressStyles)}>
        <div class="bar-inner"></div>
      </div>
      <div class="bar secondary-bar">
        <div class="bar-inner"></div>
      </div>
    `;
    }
}
__decorate([
    property({ type: Number })
], LinearProgress.prototype, "buffer", void 0);
//# sourceMappingURL=linear-progress.js.map