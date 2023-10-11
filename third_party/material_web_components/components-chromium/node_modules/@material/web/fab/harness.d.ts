/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Harness } from '../testing/harness.js';
import { Fab } from './internal/fab.js';
/**
 * Test harness for floating action buttons.
 */
export declare class FabHarness extends Harness<Fab> {
    getInteractiveElement(): Promise<HTMLButtonElement>;
}
