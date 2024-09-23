/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { CSSResultOrNative } from 'lit';
import { IconButton } from './internal/icon-button.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-filled-tonal-icon-button': MdFilledTonalIconButton;
    }
}
/**
 * @summary Icon buttons help people take supplementary actions with a single
 * tap.
 *
 * @description
 * __Emphasis:__ Low emphasis – For optional or supplementary actions with the
 * least amount of prominence.
 *
 * __Rationale:__ The most compact and unobtrusive type of button, icon buttons
 * are used for optional supplementary actions such as "Bookmark" or "Star."
 *
 * __Example usages:__
 * - Add to Favorites
 * - Print
 *
 * @final
 * @suppress {visibility}
 */
export declare class MdFilledTonalIconButton extends IconButton {
    static styles: CSSResultOrNative[];
    protected getRenderClasses(): {
        'filled-tonal': boolean;
        'toggle-filled-tonal': boolean;
        'flip-icon': boolean;
        selected: boolean;
    };
}
