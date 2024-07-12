/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { FilledTonalButton } from './internal/filled-tonal-button.js';
import { styles as tonalStyles } from './internal/filled-tonal-styles.js';
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
 * __Rationale:__ Filled tonal buttons have a lighter background color and
 * darker label color, making them less visually prominent than a regular,
 * filled button. They’re still used for final or unblocking actions in a flow,
 * but do so with less emphasis.
 *
 * __Example usages:__
 * - Save
 * - Confirm
 * - Done
 *
 * @final
 * @suppress {visibility}
 */
export let MdFilledTonalButton = class MdFilledTonalButton extends FilledTonalButton {
};
MdFilledTonalButton.styles = [
    sharedStyles,
    sharedElevationStyles,
    tonalStyles,
];
MdFilledTonalButton = __decorate([
    customElement('md-filled-tonal-button')
], MdFilledTonalButton);
//# sourceMappingURL=filled-tonal-button.js.map