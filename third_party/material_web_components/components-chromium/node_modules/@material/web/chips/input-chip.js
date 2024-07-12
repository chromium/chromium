/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { InputChip } from './internal/input-chip.js';
import { styles } from './internal/input-styles.js';
import { styles as selectableStyles } from './internal/selectable-styles.js';
import { styles as sharedStyles } from './internal/shared-styles.js';
import { styles as trailingIconStyles } from './internal/trailing-icon-styles.js';
/**
 * TODO(b/243982145): add docs
 *
 * @final
 * @suppress {visibility}
 */
export let MdInputChip = class MdInputChip extends InputChip {
};
MdInputChip.styles = [
    sharedStyles,
    trailingIconStyles,
    selectableStyles,
    styles,
];
MdInputChip = __decorate([
    customElement('md-input-chip')
], MdInputChip);
//# sourceMappingURL=input-chip.js.map