/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { OutlinedSelect } from './internal/outlined-select.js';
import { styles } from './internal/outlined-select-styles.js';
import { styles as sharedStyles } from './internal/shared-styles.js';
/**
 * @summary
 * Select menus display a list of choices on temporary surfaces and display the
 * currently selected menu item above the menu.
 *
 * @description
 * The select component allows users to choose a value from a fixed list of
 * available options. Composed of an interactive anchor button and a menu, it is
 * analogous to the native HTML `<select>` element. This is the "outlined"
 * variant.
 *
 * @example
 * ```html
 * <md-outlined-select label="fruits">
 *   <!-- An empty selected option will give select an "un-filled" state -->
 *   <md-select-option selected></md-select-option>
 *   <md-select-option value="apple" headline="Apple"></md-select-option>
 *   <md-select-option value="banana" headline="Banana"></md-select-option>
 *   <md-select-option value="kiwi" headline="Kiwi"></md-select-option>
 *   <md-select-option value="orange" headline="Orange"></md-select-option>
 *   <md-select-option value="tomato" headline="Tomato"></md-select-option>
 * </md-outlined-select>
 * ```
 *
 * @final
 * @suppress {visibility}
 */
export let MdOutlinedSelect = class MdOutlinedSelect extends OutlinedSelect {
};
MdOutlinedSelect.styles = [sharedStyles, styles];
MdOutlinedSelect = __decorate([
    customElement('md-outlined-select')
], MdOutlinedSelect);
//# sourceMappingURL=outlined-select.js.map