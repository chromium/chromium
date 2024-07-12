/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { Fab } from './internal/fab.js';
import { styles } from './internal/fab-styles.js';
import { styles as forcedColors } from './internal/forced-colors-styles.js';
import { styles as sharedStyles } from './internal/shared-styles.js';
/**
 * @summary Floating action buttons (FABs) help people take primary actions.
 * They’re used to represent the most important action on a screen, such as
 * Create or Reply.
 *
 * @description
 * __Emphasis:__ High emphasis – For the primary, most important, or most common
 * action on a screen
 *
 * __Rationale:__ The FAB remains the default component for a screen’s primary
 * action. It comes in three sizes: small FAB, FAB, and large FAB. The extended
 * FAB’s wider format and text label give it more visual prominence than a  FAB.
 * It’s often used on larger screens where a FAB would seem too small.
 *
 * __Example usages:__
 * - FAB
 *   - Create
 *   - Compose
 * - Extended FAB
 *   - Create
 *   - Compose
 *   - New Thread
 *   - New File
 *
 * @final
 * @suppress {visibility}
 */
export let MdFab = class MdFab extends Fab {
};
MdFab.styles = [sharedStyles, styles, forcedColors];
MdFab = __decorate([
    customElement('md-fab')
], MdFab);
//# sourceMappingURL=fab.js.map