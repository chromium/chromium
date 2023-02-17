/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { TemplateResult } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
import { FabShared } from './fab-shared.js';
/**
 * @soyCompatible
 */
export declare class FabExtended extends FabShared {
    /** @soyTemplate */
    protected getRenderClasses(): ClassInfo;
    /** @soyTemplate */
    protected renderIcon(icon: string): TemplateResult | string;
    /** @soyTemplate */
    protected renderLabel(): TemplateResult;
}
