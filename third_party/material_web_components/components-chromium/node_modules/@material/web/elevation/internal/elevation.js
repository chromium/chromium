/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { html, LitElement } from 'lit';
/**
 * A component for elevation.
 */
export class Elevation extends LitElement {
    connectedCallback() {
        super.connectedCallback();
        // Needed for VoiceOver, which will create a "group" if the element is a
        // sibling to other content.
        this.setAttribute('aria-hidden', 'true');
    }
    render() {
        return html `<span class="shadow"></span>`;
    }
}
//# sourceMappingURL=elevation.js.map