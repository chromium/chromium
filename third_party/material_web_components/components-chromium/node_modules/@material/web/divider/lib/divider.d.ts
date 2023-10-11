/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement } from 'lit';
/**
 * A divider component.
 */
export declare class Divider extends LitElement {
    /**
     * Indents the divider with equal padding on both sides.
     */
    inset: boolean;
    /**
     * Indents the divider with padding on the leading side.
     */
    insetStart: boolean;
    /**
     * Indents the divider with padding on the trailing side.
     */
    insetEnd: boolean;
}
