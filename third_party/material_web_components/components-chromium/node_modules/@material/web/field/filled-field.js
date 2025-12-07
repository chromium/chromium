/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { FilledField } from './internal/filled-field.js';
import { styles as filledStyles } from './internal/filled-styles.js';
import { styles as sharedStyles } from './internal/shared-styles.js';
/**
 * TODO(b/228525797): add docs
 * @final
 * @suppress {visibility}
 */
let MdFilledField = class MdFilledField extends FilledField {
};
MdFilledField.styles = [sharedStyles, filledStyles];
MdFilledField = __decorate([
    customElement('md-filled-field')
], MdFilledField);
export { MdFilledField };
//# sourceMappingURL=filled-field.js.map