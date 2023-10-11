/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { TemplateResult } from 'lit';
import { Field } from './field.js';
/**
 * An outlined field component.
 */
export declare class OutlinedField extends Field {
    protected renderOutline(floatingLabel: TemplateResult): TemplateResult<1>;
}
