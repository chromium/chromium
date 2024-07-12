/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { CSSResultOrNative } from 'lit';
import { FilledSelect } from './internal/filled-select.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-filled-select': MdFilledSelect;
    }
}
/**
 * @summary
 * Select menus display a list of choices on temporary surfaces and display the
 * currently selected menu item above the menu.
 *
 * @description
 * The select component allows users to choose a value from a fixed list of
 * available options. Composed of an interactive anchor button and a menu, it is
 * analogous to the native HTML `<select>` element. This is the "filled"
 * variant.
 *
 * @example
 * ```html
 * <md-filled-select label="fruits">
 *   <!-- An empty selected option will give select an "un-filled" state -->
 *   <md-select-option selected></md-select-option>
 *   <md-select-option value="apple" headline="Apple"></md-select-option>
 *   <md-select-option value="banana" headline="Banana"></md-select-option>
 *   <md-select-option value="kiwi" headline="Kiwi"></md-select-option>
 *   <md-select-option value="orange" headline="Orange"></md-select-option>
 *   <md-select-option value="tomato" headline="Tomato"></md-select-option>
 * </md-filled-select>
 * ```
 *
 * @final
 * @suppress {visibility}
 */
export declare class MdFilledSelect extends FilledSelect {
    static styles: CSSResultOrNative[];
}
