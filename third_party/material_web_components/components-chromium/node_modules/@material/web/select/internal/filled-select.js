/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../field/filled-field.js';
import { literal } from 'lit/static-html.js';
import { Select } from './select.js';
// tslint:disable-next-line:enforce-comments-on-exported-symbols
export class FilledSelect extends Select {
    constructor() {
        super(...arguments);
        this.fieldTag = literal `md-filled-field`;
    }
}
//# sourceMappingURL=filled-select.js.map