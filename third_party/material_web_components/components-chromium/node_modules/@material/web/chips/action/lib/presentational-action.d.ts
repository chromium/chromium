/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { TemplateResult } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
import { PrimaryAction } from './primary-action.js';
/** @soyCompatible */
export declare class PresentationalAction extends PrimaryAction {
    /** @soyTemplate */
    protected render(): TemplateResult;
    /** @soyTemplate */
    protected getRootClasses(): ClassInfo;
}
