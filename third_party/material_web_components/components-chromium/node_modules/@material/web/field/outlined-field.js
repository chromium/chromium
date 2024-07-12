/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { OutlinedField } from './internal/outlined-field.js';
import { styles as outlinedStyles } from './internal/outlined-styles.js';
import { styles as sharedStyles } from './internal/shared-styles.js';
/**
 * TODO(b/228525797): add docs
 * @final
 * @suppress {visibility}
 */
export let MdOutlinedField = class MdOutlinedField extends OutlinedField {
};
MdOutlinedField.styles = [sharedStyles, outlinedStyles];
MdOutlinedField = __decorate([
    customElement('md-outlined-field')
], MdOutlinedField);
//# sourceMappingURL=outlined-field.js.map