// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @const {string} filesSpinnerTemplate
 */
const filesSpinnerTemplate = `
  <style>
    :host([hidden]) {
      display: none !important;
    }

    :host {
      display: flex;
      height: 24px;
      margin: 12px;
      width: 24px;
    }

    svg {
      animation: rotate 1.67s linear infinite;
      transform-origin: 50% 50%;
    }

    @keyframes rotate {
      to {
        transform: rotate(360deg);
      }
    }

    circle {
      animation: spin 1.34s ease infinite;
      stroke-dasharray: 65;
      stroke-linecap: round;
      stroke-width: 3px;
      stroke: var(--google-blue-600, #1a73e8);
      transform-origin: 50% 50%;
    }

    @keyframes spin {
      0% {
        stroke-dashoffset: 64;
      }

      58% {
        stroke-dashoffset: 19;
        transform: rotate(50deg);
      }

      to {
        stroke-dashoffset: 64;
        transform: rotate(360deg);
      }
    }
  </style>

  <svg width='24' height='24' viewBox='0 0 24 24'>
    <circle cx='12' cy='12' r='10' fill='none'></circle>
  </svg>
`;

/**
 * FilesSpinner.
 */
/* #export */ class FilesSpinner extends HTMLElement {
  constructor() {
    super().attachShadow({mode: 'open'}).innerHTML = filesSpinnerTemplate;
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
