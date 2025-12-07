/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { html } from 'lit';
import { Field } from './field.js';
/**
 * A filled field component.
 */
export class FilledField extends Field {
    renderBackground() {
        return html ` <div class="background"></div> `;
    }
    renderStateLayer() {
        return html ` <div class="state-layer"></div> `;
    }
    renderIndicator() {
        return html `<div class="active-indicator"></div>`;
    }
}
//# sourceMappingURL=filled-field.js.map