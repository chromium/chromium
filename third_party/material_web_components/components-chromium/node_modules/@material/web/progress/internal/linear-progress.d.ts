/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Progress } from './progress.js';
/**
 * A linear progress component.
 */
export declare class LinearProgress extends Progress {
    /**
     * Buffer amount to display, a fraction between 0 and `max`.
     * If the value is 0 or negative, the buffer is not displayed.
     */
    buffer: number;
    protected renderIndicator(): import("lit-html").TemplateResult<1>;
}
