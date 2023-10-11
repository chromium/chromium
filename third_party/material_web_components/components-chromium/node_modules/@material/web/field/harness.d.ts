/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Harness } from '../testing/harness.js';
import { Field } from './internal/field.js';
/**
 * Test harness for field elements.
 */
export declare class FieldHarness extends Harness<Field> {
    focusWithKeyboard(init?: KeyboardEventInit): Promise<void>;
    focusWithPointer(): Promise<void>;
    blur(): Promise<void>;
    protected getInteractiveElement(): Promise<HTMLElement>;
}
