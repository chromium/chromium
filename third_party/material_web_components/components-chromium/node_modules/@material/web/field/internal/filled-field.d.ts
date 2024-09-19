/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Field } from './field.js';
/**
 * A filled field component.
 */
export declare class FilledField extends Field {
    protected renderBackground(): import("lit-html").TemplateResult<1>;
    protected renderStateLayer(): import("lit-html").TemplateResult<1>;
    protected renderIndicator(): import("lit-html").TemplateResult<1>;
}
