/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../field/filled-field.js';
import { literal } from 'lit/static-html.js';
import { TextField } from './text-field.js';
/**
 * A filled text field component.
 */
export class FilledTextField extends TextField {
    constructor() {
        super(...arguments);
        this.fieldTag = literal `md-filled-field`;
    }
}
//# sourceMappingURL=filled-text-field.js.map