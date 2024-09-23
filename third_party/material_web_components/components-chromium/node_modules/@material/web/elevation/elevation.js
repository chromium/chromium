/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { Elevation } from './internal/elevation.js';
import { styles } from './internal/elevation-styles.js';
/**
 * The `<md-elevation>` custom element with default styles.
 *
 * Elevation is the relative distance between two surfaces along the z-axis.
 *
 * @final
 * @suppress {visibility}
 */
export let MdElevation = class MdElevation extends Elevation {
};
MdElevation.styles = [styles];
MdElevation = __decorate([
    customElement('md-elevation')
], MdElevation);
//# sourceMappingURL=elevation.js.map