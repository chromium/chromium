/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { Divider } from './internal/divider.js';
import { styles } from './internal/divider-styles.js';
/**
 * @summary A divider is a thin line that groups content in lists and
 * containers.
 *
 * @description Dividers can reinforce tapability, such as when used to separate
 * list items or define tappable regions in an accordion.
 *
 * @final
 * @suppress {visibility}
 */
export let MdDivider = class MdDivider extends Divider {
};
MdDivider.styles = [styles];
MdDivider = __decorate([
    customElement('md-divider')
], MdDivider);
//# sourceMappingURL=divider.js.map