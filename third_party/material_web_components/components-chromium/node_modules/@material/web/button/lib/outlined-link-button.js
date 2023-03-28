/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { html } from 'lit';
import { LinkButton } from './link-button.js';
// tslint:disable-next-line:enforce-comments-on-exported-symbols
export class OutlinedLinkButton extends LinkButton {
    getRenderClasses() {
        return {
            ...super.getRenderClasses(),
            'md3-button--outlined': true,
        };
    }
    renderOutline() {
        return html `<span class="md3-button__outline"></span>`;
    }
}
//# sourceMappingURL=outlined-link-button.js.map