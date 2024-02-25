/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement } from 'lit';
/**
 * An item layout component.
 */
export declare class Item extends LitElement {
    /**
     * Only needed for SSR.
     *
     * Add this attribute when an item has two lines to avoid a Flash Of Unstyled
     * Content. This attribute is not needed for single line items or items with
     * three or more lines.
     */
    multiline: boolean;
    private readonly textSlots;
    render(): import("lit-html").TemplateResult<1>;
    private handleTextSlotChange;
}
