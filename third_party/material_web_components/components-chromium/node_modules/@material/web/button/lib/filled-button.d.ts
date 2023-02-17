/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../elevation/elevation.js';
import { TemplateResult } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
import { Button } from './button.js';
export declare class FilledButton extends Button {
    protected getRenderClasses(): ClassInfo;
    /** @soyTemplate */
    protected renderElevation(): TemplateResult;
}
