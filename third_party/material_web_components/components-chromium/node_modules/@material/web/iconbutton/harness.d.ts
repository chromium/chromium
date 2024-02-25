/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Harness } from '../testing/harness.js';
import { IconButton } from './internal/icon-button.js';
/**
 * Test harness for icon buttons.
 */
export declare class IconButtonHarness extends Harness<IconButton> {
    protected getInteractiveElement(): Promise<HTMLAnchorElement | HTMLButtonElement>;
}
