/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { TemplateResult } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
import { SegmentedButton } from './segmented-button.js';
/** @soyCompatible */
export declare class OutlinedSegmentedButton extends SegmentedButton {
    /** @soyTemplate */
    protected getRenderClasses(): ClassInfo;
    /** @soyTemplate */
    protected renderOutline(): TemplateResult;
}
