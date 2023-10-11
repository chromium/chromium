/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Harness } from '../testing/harness.js';
import { Dialog } from './internal/dialog.js';
/**
 * Test harness for dialog.
 */
export declare class DialogHarness extends Harness<Dialog> {
    getInteractiveElement(): Promise<HTMLElement>;
}
