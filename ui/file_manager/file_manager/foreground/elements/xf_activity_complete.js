// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Activity complete indicator custom element for use in PanelItem(s).
 */
class ActivityComplete extends HTMLElement {
  constructor() {
    super();
    const host = document.createElement('template');
    host.innerHTML = this.constructor.template_;
    this.attachShadow({mode: 'open'}).appendChild(host.content.cloneNode(true));
  }

  /**
   * Static getter for the custom element template.
   * @return {string}
   * @private
   */
  static get template_() {
    return `<style>
                    .complete {
                        height: 36px;
                        width: 36px;
                        stroke-width: 3px;
                        fill: none;
                    }
                </style>
                <div class='complete'>
                    <svg xmlns='http://www.w3.org/2000/svg'
                        viewBox='0 0 36 36'>
                        <path id='success' display='inherit'
                            d='M4 20l9 9l22 -22' stroke='#34A853'/>
                        <g id='failure' display='none' stroke='#EA4335'>
                            <circle cx='18' cy='18' r='14'/>
                            <path d='M18 10l0 10M18 23l0 3'/>
                        </g>
                    </svg>
                </div>`;
  }

  /**
   * Registers this instance to listen to these attribute changes.
   * @return {!Array<string>}
   * @private
   */
  static get observedAttributes() {
    return ['status'];
  }

  /**
   * Callback triggered by the browser when our attribute values change.
   * @param {string} name Attribute that's changed.
   * @param {?string} oldValue Old value of the attribute.
   * @param {?string} newValue New value of the attribute.
   * @private
   */
  attributeChangedCallback(name, oldValue, newValue) {
    if (name === 'status') {
      if (oldValue != newValue) {
        const success = this.shadowRoot.querySelector('#success');
        const failure = this.shadowRoot.querySelector('#failure');
        switch (newValue) {
          case 'success':
            success.setAttribute('display', 'inherit');
            failure.setAttribute('display', 'none');
            break;

          case 'failure':
            failure.setAttribute('display', 'inherit');
            success.setAttribute('display', 'none');
            break;

          default:
            assertNotReached();
            break;
        }
      }
    }
  }

  /**
   * Getter for the current state of the progress indication.
   * @return {string}
   * @public
   */
  get status() {
    return this.getAttribute('status');
  }

  /**
   * Setter to set the success/failure indication.
   * @param {string} status Status value being set.
   * @public
   */
  set status(status) {
    // Reflect the status property into the attribute.
    this.setAttribute('status', status);
  }
}

window.customElements.define('xf-activity-complete', ActivityComplete);
