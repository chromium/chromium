/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { TemplateResult } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
import { Field } from './field.js';
/** @soyCompatible */
export declare class OutlinedField extends Field {
    /** @soyTemplate */
    protected getRenderClasses(): ClassInfo;
    /** @soyTemplate */
    protected renderContainerContents(): TemplateResult;
    /** @soyTemplate */
    protected renderOutline(): TemplateResult;
    /** @soyTemplate */
    protected renderMiddleContents(): TemplateResult;
}
