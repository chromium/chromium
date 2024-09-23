/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { CSSResultOrNative } from 'lit';
import { FocusRing } from './internal/focus-ring.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-focus-ring': MdFocusRing;
    }
}
/**
 * TODO(b/267336424): add docs
 *
 * @final
 * @suppress {visibility}
 */
export declare class MdFocusRing extends FocusRing {
    static styles: CSSResultOrNative[];
}
