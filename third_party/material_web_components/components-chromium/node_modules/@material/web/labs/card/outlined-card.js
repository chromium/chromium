/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { Card } from './internal/card.js';
import { styles as outlinedStyles } from './internal/outlined-styles.js';
import { styles as sharedStyles } from './internal/shared-styles.js';
/**
 * @final
 * @suppress {visibility}
 */
let MdOutlinedCard = class MdOutlinedCard extends Card {
};
MdOutlinedCard.styles = [sharedStyles, outlinedStyles];
MdOutlinedCard = __decorate([
    customElement('md-outlined-card')
], MdOutlinedCard);
export { MdOutlinedCard };
//# sourceMappingURL=outlined-card.js.map