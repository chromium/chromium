// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A button used inside PanelItem with varying display characteristics.
 */
class PanelButton extends HTMLElement {
  constructor() {
    super();
    this.createElement_();
  }

  /**
   * Creates a PanelButton.
   * @private
   */
  createElement_() {
    const template = document.createElement('template');
    template.innerHTML = PanelButton.html_();
    const fragment = template.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);
  }

  /**
   * Get the custom element template string.
   * @private
   * @return {string}
   */
  static html_() {
    return `<style>
              cr-icon-button, cr-button {
                margin-inline-start: 0px;
              }

              @keyframes setcollapse {
                from {
                  transform: rotate(0deg);
                }
                to {
                  transform: rotate(180deg);
                }
              }

              @keyframes setexpand {
                from {
                  transform: rotate(-180deg);
                }
                to {
                  transform: rotate(0deg);
                }
              }

              :host([data-category='expand']) {
                  animation: setexpand 200ms forwards;
              }

              :host([data-category='collapse']) {
                  animation: setcollapse 200ms forwards;
              }

              :host {
                position: relative;
              }

              :host(:not([data-category='dismiss'])) {
                width: 36px;
              }

              :host([data-category='dismiss']) #icon {
                display: none;
              }

              :host(:not([data-category='dismiss'])) #dismiss {
                display: none;
              }
            </style>
            <cr-button id='dismiss'>$i18n{DRIVE_WELCOME_DISMISS}</cr-button>
            <cr-icon-button id='icon'></cr-icon-button>`;
  }

  /**
   * Registers this instance to listen to these attribute changes.
   * @private
   */
  static get observedAttributes() {
    return [
      'data-category',
    ];
  }

  /**
   * Callback triggered by the browser when our attribute values change.
   * @param {string} name Attribute that's changed.
   * @param {?string} oldValue Old value of the attribute.
   * @param {?string} newValue New value of the attribute.
   * @private
   */
  attributeChangedCallback(name, oldValue, newValue) {
    if (oldValue === newValue) {
      return;
    }
    /** @type {Element} */
    const iconButton = this.shadowRoot.querySelector('cr-icon-button');
    if (name === 'data-category') {
      switch (newValue) {
        case 'cancel':
          iconButton.setAttribute('iron-icon', 'cr:clear');
          break;
        case 'collapse':
        case 'expand':
          iconButton.setAttribute('iron-icon', 'cr:expand-less');
          break;
      }
    }
  }
}

window.customElements.define('xf-button', PanelButton);
