/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Harness } from '../testing/harness.js';
import { Slider } from './internal/slider.js';
/**
 * Test harness for slider.
 */
export declare class SliderHarness extends Harness<Slider> {
    getInteractiveElement(): Promise<HTMLInputElement>;
    getInputs(): HTMLInputElement[];
    getHandles(): Element[];
    getLabels(): Element[];
    isLabelShowing(): boolean;
    simulateValueInteraction(value: number, el?: HTMLInputElement): Promise<void>;
    private positionEventAtHandle;
    protected simulateStartHover(element: HTMLElement, init?: PointerEventInit): void;
    protected simulateMousePress(element: HTMLElement, init?: PointerEventInit): void;
}
