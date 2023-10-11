/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { property } from 'lit/decorators.js';
import { Tab } from './tab.js';
/**
 * A primary tab component.
 */
export class PrimaryTab extends Tab {
    constructor() {
        super(...arguments);
        /**
         * Whether or not the icon renders inline with label or stacked vertically.
         */
        this.inlineIcon = false;
    }
    getContentClasses() {
        return {
            ...super.getContentClasses(),
            'stacked': !this.inlineIcon,
        };
    }
}
__decorate([
    property({ type: Boolean, attribute: 'inline-icon' })
], PrimaryTab.prototype, "inlineIcon", void 0);
//# sourceMappingURL=primary-tab.js.map