/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { CSSResultOrNative } from 'lit';
import { LinearProgress } from './internal/linear-progress.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-linear-progress': MdLinearProgress;
    }
}
/**
 * @summary Linear progress indicators display progress by animating along the
 * length of a fixed, visible track.
 *
 * @description
 * Progress indicators inform users about the status of ongoing processes.
 * - Determinate indicators display how long a process will take.
 * - Indeterminate indicators express an unspecified amount of wait time.
 *
 * @final
 * @suppress {visibility}
 */
export declare class MdLinearProgress extends LinearProgress {
    static styles: CSSResultOrNative[];
}
