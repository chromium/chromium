/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { AssistChip } from './internal/assist-chip.js';
import { styles } from './internal/assist-styles.css.js';
import { styles as elevatedStyles } from './internal/elevated-styles.css.js';
import { styles as sharedStyles } from './internal/shared-styles.css.js';
/**
 * TODO(b/243982145): add docs
 *
 * @final
 * @suppress {visibility}
 */
export let MdAssistChip = class MdAssistChip extends AssistChip {
};
MdAssistChip.styles = [sharedStyles, elevatedStyles, styles];
MdAssistChip = __decorate([
    customElement('md-assist-chip')
], MdAssistChip);
//# sourceMappingURL=assist-chip.js.map