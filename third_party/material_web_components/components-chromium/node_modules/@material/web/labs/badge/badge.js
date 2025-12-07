/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { Badge } from './internal/badge.js';
import { styles } from './internal/badge-styles.js';
/**
 * @final
 * @suppress {visibility}
 */
let MdBadge = class MdBadge extends Badge {
};
MdBadge.styles = [styles];
MdBadge = __decorate([
    customElement('md-badge')
], MdBadge);
export { MdBadge };
//# sourceMappingURL=badge.js.map