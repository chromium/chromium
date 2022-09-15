// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @type {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

/**
 * FilesSpinner.
 */
export class FilesSpinner extends HTMLElement {
  constructor() {
    super();

    const fragment = htmlTemplate.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);
  }

  /**
   * DOM connected: set aria attributes.
   */
  connectedCallback() {
    const host = /** @type {!HTMLElement} */ (this.shadowRoot.host);

    host.setAttribute('role', 'progressbar');
    host.setAttribute('aria-disabled', 'false');
    host.setAttribute('aria-valuemin', '0');
    host.setAttribute('aria-valuemax', '1');
  }
}

customElements.define('files-spinner', FilesSpinner);

//# sourceURL=//ui/file_manager/file_manager/foreground/elements/files_spinner.js
