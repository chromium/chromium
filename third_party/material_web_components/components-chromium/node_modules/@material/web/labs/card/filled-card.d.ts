/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { CSSResultOrNative } from 'lit';
import { Card } from './internal/card.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-filled-card': MdFilledCard;
    }
}
/**
 * @final
 * @suppress {visibility}
 */
export declare class MdFilledCard extends Card {
    static styles: CSSResultOrNative[];
}
