/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Harness } from '../testing/harness.js';
import { Chip } from './internal/chip.js';
/**
 * Test harness for chips.
 */
export declare class ChipHarness extends Harness<Chip> {
    action: 'primary' | 'trailing';
    protected getInteractiveElement(): Promise<HTMLElement>;
}
