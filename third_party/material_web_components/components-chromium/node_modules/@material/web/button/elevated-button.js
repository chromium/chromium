/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { ElevatedButton } from './internal/elevated-button.js';
import { styles as elevatedStyles } from './internal/elevated-styles.js';
import { styles as sharedElevationStyles } from './internal/shared-elevation-styles.js';
import { styles as sharedStyles } from './internal/shared-styles.js';
/**
 * @summary Buttons help people take action, such as sending an email, sharing a
 * document, or liking a comment.
 *
 * @description
 * __Emphasis:__ Medium emphasis – For important actions that don’t distract
 * from other onscreen elements.
 *
 * __Rationale:__ Elevated buttons are essentially filled buttons with a lighter
 * background color and a shadow. To prevent shadow creep, only use them when
 * absolutely necessary, such as when the button requires visual separation from
 * a patterned background.
 *
 * __Example usages:__
 * - Reply
 * - View all
 * - Add to cart
 * - Take out of trash
 *
 * @final
 * @suppress {visibility}
 */
let MdElevatedButton = class MdElevatedButton extends ElevatedButton {
};
MdElevatedButton.styles = [
    sharedStyles,
    sharedElevationStyles,
    elevatedStyles,
];
MdElevatedButton = __decorate([
    customElement('md-elevated-button')
], MdElevatedButton);
export { MdElevatedButton };
//# sourceMappingURL=elevated-button.js.map