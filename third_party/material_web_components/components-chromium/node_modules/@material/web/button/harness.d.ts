/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Harness } from '../testing/harness.js';
import { Button } from './internal/button.js';
/**
 * Test harness for buttons.
 */
export declare class ButtonHarness extends Harness<Button> {
    protected getInteractiveElement(): Promise<HTMLElement>;
}
