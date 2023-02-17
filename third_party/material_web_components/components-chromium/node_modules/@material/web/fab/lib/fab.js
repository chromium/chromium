/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { FabShared } from './fab-shared.js';
/**
 * @soyCompatible
 */
export class Fab extends FabShared {
    /** @soyTemplate */
    getRenderClasses() {
        return {
            ...super.getRenderClasses(),
            'md3-fab--regular': true,
        };
    }
    /** @soyTemplate */
    renderIcon(icon) {
        return '';
    }
}
//# sourceMappingURL=fab.js.map