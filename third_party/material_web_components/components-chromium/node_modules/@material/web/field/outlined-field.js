/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { OutlinedField } from './lib/outlined-field.js';
import { styles as outlinedStyles } from './lib/outlined-styles.css.js';
import { styles as sharedStyles } from './lib/shared-styles.css.js';
/**
 * @soyCompatible
 * @final
 * @suppress {visibility}
 */
let MdOutlinedField = class MdOutlinedField extends OutlinedField {
};
MdOutlinedField.styles = [sharedStyles, outlinedStyles];
MdOutlinedField = __decorate([
    customElement('md-outlined-field')
], MdOutlinedField);
export { MdOutlinedField };
//# sourceMappingURL=outlined-field.js.map