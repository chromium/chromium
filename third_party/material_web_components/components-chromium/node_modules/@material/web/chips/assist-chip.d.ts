/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { CSSResultOrNative } from 'lit';
import { AssistChip } from './internal/assist-chip.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-assist-chip': MdAssistChip;
    }
}
/**
 * TODO(b/243982145): add docs
 *
 * @final
 * @suppress {visibility}
 */
export declare class MdAssistChip extends AssistChip {
    static styles: CSSResultOrNative[];
}
