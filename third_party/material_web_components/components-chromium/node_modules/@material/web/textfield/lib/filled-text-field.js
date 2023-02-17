/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { TextField } from './text-field.js';
/** @soyCompatible */
export class FilledTextField extends TextField {
    /** @soyTemplate */
    getRenderClasses() {
        return {
            ...super.getRenderClasses(),
            'md3-text-field--filled': true,
        };
    }
}
//# sourceMappingURL=filled-text-field.js.map