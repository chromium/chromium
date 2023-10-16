/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Harness } from '../testing/harness.js';
import { Checkbox } from './internal/checkbox.js';
/**
 * Test harness for checkbox.
 */
export declare class CheckboxHarness extends Harness<Checkbox> {
    getInteractiveElement(): Promise<HTMLInputElement>;
}
