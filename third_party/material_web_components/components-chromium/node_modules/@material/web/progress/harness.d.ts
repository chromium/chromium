/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Harness } from '../testing/harness.js';
import { CircularProgress } from './internal/circular-progress.js';
import { LinearProgress } from './internal/linear-progress.js';
/**
 * Test harness for linear-progress.
 */
export declare class LinearProgressHarness extends Harness<LinearProgress> {
    getInteractiveElement(): Promise<HTMLElement>;
}
/**
 * Test harness for circular-progress.
 */
export declare class CircularProgressHarness extends Harness<CircularProgress> {
    getInteractiveElement(): Promise<HTMLElement>;
}
