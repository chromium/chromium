/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { html, render } from 'lit';
import { Validator } from './validator.js';
/**
 * A validator that provides constraint validation that emulates `<select>`
 * validation.
 */
export class SelectValidator extends Validator {
    computeValidity(state) {
        if (!this.selectControl) {
            // Lazily create the platform select
            this.selectControl = document.createElement('select');
        }
        render(html `<option value=${state.value}></option>`, this.selectControl);
        this.selectControl.value = state.value;
        this.selectControl.required = state.required;
        return {
            validity: this.selectControl.validity,
            validationMessage: this.selectControl.validationMessage,
        };
    }
    equals(prev, next) {
        return prev.value === next.value && prev.required === next.required;
    }
    copy({ value, required }) {
        return { value, required };
    }
}
//# sourceMappingURL=select-validator.js.map