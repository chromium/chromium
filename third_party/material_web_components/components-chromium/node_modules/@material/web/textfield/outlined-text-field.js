/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import '../field/outlined-field.js';
import { customElement } from 'lit/decorators.js';
import { literal } from 'lit/static-html.js';
import { styles as outlinedStyles } from './internal/outlined-styles.js';
import { OutlinedTextField } from './internal/outlined-text-field.js';
import { styles as sharedStyles } from './internal/shared-styles.js';
/**
 * TODO(b/228525797): Add docs
 * @final
 * @suppress {visibility}
 */
export let MdOutlinedTextField = class MdOutlinedTextField extends OutlinedTextField {
    constructor() {
        super(...arguments);
        this.fieldTag = literal `md-outlined-field`;
    }
};
MdOutlinedTextField.styles = [sharedStyles, outlinedStyles];
MdOutlinedTextField = __decorate([
    customElement('md-outlined-text-field')
], MdOutlinedTextField);
//# sourceMappingURL=outlined-text-field.js.map