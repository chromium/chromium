/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { Card } from './internal/card.js';
import { styles as elevatedStyles } from './internal/elevated-styles.js';
import { styles as sharedStyles } from './internal/shared-styles.js';
/**
 * @final
 * @suppress {visibility}
 */
let MdElevatedCard = class MdElevatedCard extends Card {
};
MdElevatedCard.styles = [sharedStyles, elevatedStyles];
MdElevatedCard = __decorate([
    customElement('md-elevated-card')
], MdElevatedCard);
export { MdElevatedCard };
//# sourceMappingURL=elevated-card.js.map