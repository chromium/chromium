/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { PrimaryTab } from './internal/primary-tab.js';
import { styles as primaryStyles } from './internal/primary-tab-styles.js';
import { styles as sharedStyles } from './internal/tab-styles.js';
// TODO(b/267336507): add docs
/**
 * @summary Tab allow users to display a tab within a Tabs.
 *
 * @final
 * @suppress {visibility}
 */
export let MdPrimaryTab = class MdPrimaryTab extends PrimaryTab {
};
MdPrimaryTab.styles = [sharedStyles, primaryStyles];
MdPrimaryTab = __decorate([
    customElement('md-primary-tab')
], MdPrimaryTab);
//# sourceMappingURL=primary-tab.js.map