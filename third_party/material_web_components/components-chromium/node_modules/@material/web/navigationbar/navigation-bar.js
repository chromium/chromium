/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { NavigationBar } from './lib/navigation-bar.js';
import { styles } from './lib/navigation-bar-styles.css.js';
/**
 * @soyCompatible
 * @final
 * @suppress {visibility}
 */
let MdNavigationBar = class MdNavigationBar extends NavigationBar {
};
MdNavigationBar.styles = [styles];
MdNavigationBar = __decorate([
    customElement('md-navigation-bar')
], MdNavigationBar);
export { MdNavigationBar };
//# sourceMappingURL=navigation-bar.js.map