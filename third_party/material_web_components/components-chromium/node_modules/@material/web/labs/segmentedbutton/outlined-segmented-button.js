/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { OutlinedSegmentedButton } from './internal/outlined-segmented-button.js';
import { styles as outlinedStyles } from './internal/outlined-styles.js';
import { styles as sharedStyles } from './internal/shared-styles.js';
/**
 * MdOutlinedSegmentedButton is the custom element for the Material
 * Design outlined segmented button component.
 * @final
 * @suppress {visibility}
 */
let MdOutlinedSegmentedButton = class MdOutlinedSegmentedButton extends OutlinedSegmentedButton {
};
MdOutlinedSegmentedButton.styles = [sharedStyles, outlinedStyles];
MdOutlinedSegmentedButton = __decorate([
    customElement('md-outlined-segmented-button')
], MdOutlinedSegmentedButton);
export { MdOutlinedSegmentedButton };
//# sourceMappingURL=outlined-segmented-button.js.map