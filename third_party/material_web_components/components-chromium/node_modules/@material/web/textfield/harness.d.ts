/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Harness } from '../testing/harness.js';
import { TextField } from './internal/text-field.js';
/**
 * Test harness for text field elements.
 */
export declare class TextFieldHarness extends Harness<TextField> {
    /** Used to track whether or not a change event should be dispatched. */
    private valueBeforeChange;
    /**
     * Simulates a user typing a value one character at a time. This will fire
     * multiple input events.
     *
     * Use focus/blur to ensure change events are fired.
     *
     * @example
     * await harness.focusWithKeyboard();
     * await harness.inputValue('value'); // input events
     * await harness.blur(); // change event
     *
     * @param value The value to simulating typing.
     */
    inputValue(value: string): Promise<void>;
    /**
     * Simulates a user deleting part of a value with the backspace key.
     * By default, the entire value is deleted. This will fire a single input
     * event.
     *
     * Use focus/blur to ensure change events are fired.
     *
     * @example
     * await harness.focusWithKeyboard();
     * await harness.deleteValue(); // input event
     * await harness.blur(); // change event
     *
     * @param beginIndex The starting index of the value to delete.
     * @param endIndex The ending index of the value to delete.
     */
    deleteValue(beginIndex?: number, endIndex?: number): Promise<void>;
    reset(): Promise<void>;
    blur(): Promise<void>;
    protected simulatePointerFocus(input: HTMLElement): void;
    protected simulateInput(element: HTMLInputElement | HTMLTextAreaElement, charactersToAppend: string, init?: InputEventInit): void;
    protected simulateDeletion(element: HTMLInputElement | HTMLTextAreaElement, beginIndex?: number, endIndex?: number, init?: InputEventInit): void;
    protected simulateChangeIfNeeded(element: HTMLInputElement | HTMLTextAreaElement): void;
    protected getInteractiveElement(): Promise<HTMLInputElement | HTMLTextAreaElement>;
}
