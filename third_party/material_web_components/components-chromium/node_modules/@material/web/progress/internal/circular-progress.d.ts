/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Progress } from './progress.js';
/**
 * A circular progress component.
 */
export declare class CircularProgress extends Progress {
    protected renderIndicator(): import("lit-html").TemplateResult<1>;
    private renderDeterminateContainer;
    private renderIndeterminateContainer;
}
