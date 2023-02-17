/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * Legacy stateful foundation class for components.
 */
export class Foundation {
    constructor(adapter) {
        this.adapter = adapter;
        this.init();
    }
    init() {
        // Subclasses should override this method to perform initialization routines
    }
}
//# sourceMappingURL=foundation.js.map