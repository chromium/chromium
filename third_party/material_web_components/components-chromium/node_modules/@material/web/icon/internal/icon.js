/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { html, LitElement } from 'lit';
/**
 * TODO(b/265336902): add docs
 */
export class Icon extends LitElement {
    render() {
        return html `<slot></slot>`;
    }
    connectedCallback() {
        super.connectedCallback();
        const ariaHidden = this.getAttribute('aria-hidden');
        if (ariaHidden === 'false') {
            // Allow the user to set `aria-hidden="false"` to create an icon that is
            // announced by screenreaders.
            this.removeAttribute('aria-hidden');
            return;
        }
        // Needed for VoiceOver, which will create a "group" if the element is a
        // sibling to other content.
        this.setAttribute('aria-hidden', 'true');
    }
}
//# sourceMappingURL=icon.js.map