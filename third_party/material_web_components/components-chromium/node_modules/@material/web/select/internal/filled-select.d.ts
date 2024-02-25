/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../field/filled-field.js';
import { Select } from './select.js';
export declare abstract class FilledSelect extends Select {
    protected readonly fieldTag: import("lit-html/static.js").StaticValue;
}
