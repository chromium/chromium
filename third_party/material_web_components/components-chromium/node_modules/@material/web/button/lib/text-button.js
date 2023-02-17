/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Button } from './button.js';
// tslint:disable-next-line:enforce-comments-on-exported-symbols
export class TextButton extends Button {
    getRenderClasses() {
        return {
            ...super.getRenderClasses(),
            'md3-button--text': true,
        };
    }
}
//# sourceMappingURL=text-button.js.map