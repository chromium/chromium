/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import { customElement } from 'lit/decorators.js';
import { Switch } from './internal/switch.js';
import { styles } from './internal/switch-styles.js';
/**
 * @summary Switches toggle the state of a single item on or off.
 *
 * @description
 * There's one type of switch in Material. Use this selection control when the
 * user needs to toggle a single item on or off.
 *
 * @final
 * @suppress {visibility}
 */
export let MdSwitch = class MdSwitch extends Switch {
};
MdSwitch.styles = [styles];
MdSwitch = __decorate([
    customElement('md-switch')
], MdSwitch);
//# sourceMappingURL=switch.js.map