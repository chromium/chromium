/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { NavigationDrawer } from './internal/navigation-drawer.js';
import { styles } from './internal/navigation-drawer-styles.js';
import { styles as sharedStyles } from './internal/shared-styles.js';
/**
 * @final
 * @suppress {visibility}
 */
let MdNavigationDrawer = class MdNavigationDrawer extends NavigationDrawer {
};
MdNavigationDrawer.styles = [sharedStyles, styles];
MdNavigationDrawer = __decorate([
    customElement('md-navigation-drawer')
], MdNavigationDrawer);
export { MdNavigationDrawer };
//# sourceMappingURL=navigation-drawer.js.map