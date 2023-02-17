/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LinkButton } from './link-button.js';
// tslint:disable-next-line:enforce-comments-on-exported-symbols
export class TextLinkButton extends LinkButton {
    getRenderClasses() {
        return {
            ...super.getRenderClasses(),
            'md3-button--text': true,
        };
    }
}
//# sourceMappingURL=text-link-button.js.map