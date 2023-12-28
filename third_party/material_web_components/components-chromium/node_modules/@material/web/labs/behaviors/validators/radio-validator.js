/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Validator } from './validator.js';
/**
 * A validator that provides constraint validation that emulates
 * `<input type="radio">` validation.
 */
export class RadioValidator extends Validator {
    computeValidity(states) {
        if (!this.radioElement) {
            // Lazily create the radio element
            this.radioElement = document.createElement('input');
            this.radioElement.type = 'radio';
            // A name is required for validation to run
            this.radioElement.name = 'group';
        }
        let isRequired = false;
        let isChecked = false;
        for (const { checked, required } of states) {
            if (required) {
                isRequired = true;
            }
            if (checked) {
                isChecked = true;
            }
        }
        // Firefox v119 doesn't compute grouped radio validation correctly while
        // they are detached from the DOM, which is why we don't render multiple
        // virtual <input>s. Instead, we can check the required/checked states and
        // grab the i18n'd validation message if the value is missing.
        this.radioElement.checked = isChecked;
        this.radioElement.required = isRequired;
        return {
            validity: {
                valueMissing: isRequired && !isChecked,
            },
            validationMessage: this.radioElement.validationMessage,
        };
    }
    equals(prevGroup, nextGroup) {
        if (prevGroup.length !== nextGroup.length) {
            return false;
        }
        for (let i = 0; i < prevGroup.length; i++) {
            const prev = prevGroup[i];
            const next = nextGroup[i];
            if (prev.checked !== next.checked || prev.required !== next.required) {
                return false;
            }
        }
        return true;
    }
    copy(states) {
        // Cast as unknown since typescript does not have enough information to
        // infer that the array always has at least one element.
        return states.map(({ checked, required }) => ({
            checked,
            required,
        }));
    }
}
//# sourceMappingURL=radio-validator.js.map