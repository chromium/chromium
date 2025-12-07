/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { Tabs } from './internal/tabs.js';
import { styles } from './internal/tabs-styles.js';
// TODO(b/267336507): add docs
/**
 * @summary Tabs displays a list of selectable tabs.
 *
 * @final
 * @suppress {visibility}
 */
let MdTabs = class MdTabs extends Tabs {
};
MdTabs.styles = [styles];
MdTabs = __decorate([
    customElement('md-tabs')
], MdTabs);
export { MdTabs };
//# sourceMappingURL=tabs.js.map