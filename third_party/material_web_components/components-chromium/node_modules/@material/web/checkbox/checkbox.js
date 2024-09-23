/**
 * @license
 * Copyright 2018 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { Checkbox } from './internal/checkbox.js';
import { styles } from './internal/checkbox-styles.js';
/**
 * @summary Checkboxes allow users to select one or more items from a set.
 * Checkboxes can turn an option on or off.
 *
 * @description
 * Use checkboxes to:
 * - Select one or more options from a list
 * - Present a list containing sub-selections
 * - Turn an item on or off in a desktop environment
 *
 * @final
 * @suppress {visibility}
 */
export let MdCheckbox = class MdCheckbox extends Checkbox {
};
MdCheckbox.styles = [styles];
MdCheckbox = __decorate([
    customElement('md-checkbox')
], MdCheckbox);
//# sourceMappingURL=checkbox.js.map