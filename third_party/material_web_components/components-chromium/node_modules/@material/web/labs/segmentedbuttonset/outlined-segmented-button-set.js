/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { OutlinedSegmentedButtonSet } from './internal/outlined-segmented-button-set.js';
import { styles as outlinedStyles } from './internal/outlined-styles.js';
import { styles as sharedStyles } from './internal/shared-styles.js';
/**
 * MdOutlinedSegmentedButtonSet is the custom element for the Material
 * Design outlined segmented button set component.
 * @final
 * @suppress {visibility}
 */
export let MdOutlinedSegmentedButtonSet = class MdOutlinedSegmentedButtonSet extends OutlinedSegmentedButtonSet {
};
MdOutlinedSegmentedButtonSet.styles = [sharedStyles, outlinedStyles];
MdOutlinedSegmentedButtonSet = __decorate([
    customElement('md-outlined-segmented-button-set')
], MdOutlinedSegmentedButtonSet);
//# sourceMappingURL=outlined-segmented-button-set.js.map