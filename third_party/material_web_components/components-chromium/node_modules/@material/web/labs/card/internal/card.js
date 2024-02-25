/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../../elevation/elevation.js';
import { html, LitElement } from 'lit';
/**
 * A card component.
 */
export class Card extends LitElement {
    render() {
        return html `
      <md-elevation part="elevation"></md-elevation>
      <div class="background"></div>
      <slot></slot>
      <div class="outline"></div>
    `;
    }
}
//# sourceMappingURL=card.js.map