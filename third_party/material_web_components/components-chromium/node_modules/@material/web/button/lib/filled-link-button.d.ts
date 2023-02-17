/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../elevation/elevation.js';
import { TemplateResult } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
import { LinkButton } from './link-button.js';
export declare class FilledLinkButton extends LinkButton {
    protected getRenderClasses(): ClassInfo;
    /** @soyTemplate */
    protected renderElevation(): TemplateResult;
}
