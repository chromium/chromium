/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { TemplateResult } from 'lit';
import { PrimaryAction } from './primary-action.js';
/** @soyCompatible */
export declare class SelectableAction extends PrimaryAction {
    selected: boolean;
    /** @soyTemplate */
    protected render(): TemplateResult;
    /** @soyTemplate */
    protected renderGraphic(): TemplateResult;
    /** @soyTemplate */
    private renderCheckMark;
}
