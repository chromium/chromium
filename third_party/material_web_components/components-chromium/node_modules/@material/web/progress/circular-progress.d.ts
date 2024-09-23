/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { CSSResultOrNative } from 'lit';
import { CircularProgress } from './internal/circular-progress.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-circular-progress': MdCircularProgress;
    }
}
/**
 * @summary Circular progress indicators display progress by animating along an
 * invisible circular track in a clockwise direction. They can be applied
 * directly to a surface, such as a button or card.
 *
 * @description
 * Progress indicators inform users about the status of ongoing processes.
 * - Determinate indicators display how long a process will take.
 * - Indeterminate indicators express an unspecified amount of wait time.
 *
 * @final
 * @suppress {visibility}
 */
export declare class MdCircularProgress extends CircularProgress {
    static styles: CSSResultOrNative[];
}
