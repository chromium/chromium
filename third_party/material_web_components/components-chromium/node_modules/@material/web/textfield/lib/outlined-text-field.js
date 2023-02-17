/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { TextField } from './text-field.js';
/** @soyCompatible */
export class OutlinedTextField extends TextField {
    /** @soyTemplate */
    getRenderClasses() {
        return {
            ...super.getRenderClasses(),
            'md3-text-field--outlined': true,
        };
    }
}
//# sourceMappingURL=outlined-text-field.js.map