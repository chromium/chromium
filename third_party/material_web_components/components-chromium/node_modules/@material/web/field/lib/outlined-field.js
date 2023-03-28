/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { html } from 'lit';
import { Field } from './field.js';
/** @soyCompatible */
export class OutlinedField extends Field {
    /** @soyTemplate */
    getRenderClasses() {
        return {
            ...super.getRenderClasses(),
            'md3-field--outlined': true,
        };
    }
    /** @soyTemplate */
    renderContainerContents() {
        return html `
      ${this.renderOutline()}
      ${super.renderContainerContents()}
    `;
    }
    /** @soyTemplate */
    renderOutline() {
        return html `
      <span class="md3-field__outline">
        <span class="md3-field__outline-start"></span>
        <span class="md3-field__outline-notch">
          <span class="md3-field__outline-panel-inactive"></span>
          <span class="md3-field__outline-panel-active"></span>
          ${this.renderFloatingLabel()}
        </span>
        <span class="md3-field__outline-end"></span>
      </span>
    `;
    }
    /** @soyTemplate */
    renderMiddleContents() {
        return html `
      ${this.renderRestingLabel()}
      ${super.renderMiddleContents()}
    `;
    }
}
//# sourceMappingURL=outlined-field.js.map