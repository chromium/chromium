/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { NavigationDrawerModal } from './internal/navigation-drawer-modal.js';
import { styles } from './internal/navigation-drawer-modal-styles.js';
import { styles as sharedStyles } from './internal/shared-styles.js';
/**
 * @final
 * @suppress {visibility}
 */
export let MdNavigationDrawerModal = class MdNavigationDrawerModal extends NavigationDrawerModal {
};
MdNavigationDrawerModal.styles = [sharedStyles, styles];
MdNavigationDrawerModal = __decorate([
    customElement('md-navigation-drawer-modal')
], MdNavigationDrawerModal);
//# sourceMappingURL=navigation-drawer-modal.js.map