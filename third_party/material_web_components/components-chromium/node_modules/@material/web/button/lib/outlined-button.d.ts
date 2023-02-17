/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { TemplateResult } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
import { Button } from './button.js';
export declare class OutlinedButton extends Button {
    protected getRenderClasses(): ClassInfo;
    protected renderOutline(): TemplateResult;
}
