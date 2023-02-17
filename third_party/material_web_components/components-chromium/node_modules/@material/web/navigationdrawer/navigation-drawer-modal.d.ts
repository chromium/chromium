/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { NavigationDrawerModal } from './lib/navigation-drawer-modal.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-navigation-drawer-modal': MdNavigationDrawerModal;
    }
}
/**
 * @soyCompatible
 * @final
 * @suppress {visibility}
 */
export declare class MdNavigationDrawerModal extends NavigationDrawerModal {
    static readonly styles: import("lit").CSSResult[];
}
