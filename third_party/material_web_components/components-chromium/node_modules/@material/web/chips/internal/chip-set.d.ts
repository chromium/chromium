/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement } from 'lit';
import { Chip } from './chip.js';
/**
 * A chip set component.
 */
export declare class ChipSet extends LitElement {
    get chips(): Chip[];
    private readonly childElements;
    private readonly internals;
    constructor();
    protected render(): import("lit-html").TemplateResult<1>;
    private handleKeyDown;
    private updateTabIndices;
}
