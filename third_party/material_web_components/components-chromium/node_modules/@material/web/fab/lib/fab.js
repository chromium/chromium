/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { property } from 'lit/decorators.js';
import { SharedFab } from './shared.js';
// tslint:disable-next-line:enforce-comments-on-exported-symbols
export class Fab extends SharedFab {
    constructor() {
        super(...arguments);
        /**
         * The FAB color variant to render.
         */
        this.variant = 'surface';
    }
    getRenderClasses() {
        return {
            ...super.getRenderClasses(),
            'primary': this.variant === 'primary',
            'secondary': this.variant === 'secondary',
            'tertiary': this.variant === 'tertiary',
        };
    }
}
__decorate([
    property()
], Fab.prototype, "variant", void 0);
//# sourceMappingURL=fab.js.map