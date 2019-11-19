// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * FilesMessage template.
 * @const @type {string}
 */
const filesMessageTemplate = `
  <style>
    :host([hidden]) {
      display: none;
    }

    :host {
      border-top: 1px solid var(--google-grey-refresh-300);
      border-bottom: 1px solid var(--google-grey-refresh-300);
      color: var(--cr-primary-text-color);
      display: flex;
      justify-content: space-between;
      white-space: nowrap;
      padding: 5px 0px;
      height: 36px;
    }

    cr-icon-button:not([iron-icon]) {
      visibility: hidden;
      width: 16px;
      margin: 0px;
    }

    cr-icon-button[iron-icon] {
      margin: 0px 8px;
    }

    cr-icon-button[role=img] {
      --iron-icon-fill-color: var(--google-red-600);
      pointer-events: none;
    }

    #text[aria-label]::after {
      content: attr(aria-label);
    }

    #text[aria-label] {
      margin-inline-end: 8px;
    }

    #dismiss:not([aria-label]), #action:not([aria-label]) {
      display: none;
    }

    #dismiss[aria-label]::after, #action[aria-label]::after {
      content: attr(aria-label);
    }

    #dismiss, #action {
      padding: 0px calc(12px - 1px);
    }
  </style>

  <div role='alert'>
    <cr-icon-button id='icon'></cr-icon-button>
    <span id='text'></span>
  </div>
  <div>
    <cr-button id='dismiss'></cr-button>
    <cr-button id='action'></cr-button>
    <cr-icon-button id='close'></cr-icon-button>
  </div>
`;

/**
 * FilesMessage.
 */
class FilesMessage extends HTMLElement {
  constructor() {
    super();
    this.createElement_();

    /**
     * FilesMessage visual signals user callback.
     * @private @type {!function(*)}
     */
    this.signal_ = console.log;
  }

  /**
   * Creates FilesMessage element.
   * @private
   */
  createElement_() {
    this.attachShadow({mode: 'open'}).innerHTML = filesMessageTemplate;
  }

  /**
   * Sets |this| properties (even its HTMLElement properties) from an object.
   * @param {!Object} object Settable property name/value pairs.
   * @public
   */
  setContent(object) {
    Object.assign(this, object);
  }

  /**
   * Sets the FilesMessage user visual signals callback.
   * @param {?function(*)} signal
   * @public
   */
  setSignalCallback(signal) {
    this.signal_ = signal || console.log;
  }

  /**
   * DOM connected.
   * @private
   */
  connectedCallback() {
    this.onclick = this.onClicked_.bind(this);
  }

  /**
   * Handles Polymer 'click' events from our sub-elements and emits visual
   * signals to the |signal_| callback if needed.
   * @param {?Event} event
   * @private
   */
  onClicked_(event) {
    event.stopImmediatePropagation();
    event.preventDefault();

    let id = assert(event.target.id);
    if (id === 'icon' || id === 'close') {
      id = event.target.getAttribute('iron-icon');
    } else if (event.target === this) {
      return;  // Ignore main element clicks.
    }

    this.signal_(id);
  }

  /**
   * Returns FilesMessage sub-element by |id|.
   * @param {string} id
   * @return {!HTMLElement}
   * @private
   */
  getShadowElement_(id) {
    return assert(this.shadowRoot.getElementById(id));
  }

  /**
   * Sets icon type, aria-label. Hides icon if |type| falsey. Note aria role
   * and tabindex are reset here since 'set info' also calls 'set icon'.
   * @param {?{
   *   icon:string,
   *   label:?string,
   * }} type
   * @public
   */
  set icon(type) {
    const element = this.getShadowElement_('icon');
    element.setAttribute('role', 'button');
    if (type) {
      element.setAttribute('aria-label', type.label || '');
      element.setAttribute('iron-icon', type.icon);
      element.setAttribute('tabindex', '0');
      element.onclick = this.onclick;
    } else {
      element.removeAttribute('iron-icon');  // hide
      element.onclick = null;
    }
  }

  /**
   * Sets icon info type, aria-label. The icon is shown, non-clickable.
   * @param {?{
   *   icon:string,
   *   label:?string,
   * }} type
   * @public
   */
  set info(type) {
    this.icon = type;  // Set icon property first.

    const element = this.getShadowElement_('icon');
    element.setAttribute('role', 'img');
    element.removeAttribute('tabindex');
    element.onclick = null;
  }

  /**
   * Sets the text message.
   * @param {?string} text
   * @public
   */
  set message(text) {
    const element = this.getShadowElement_('text');
    element.setAttribute('aria-label', text || '');
  }

  /**
   * Sets dismiss button text. Hides dismiss button if |text| falsey.
   * @param {?string} text
   * @public
   */
  set dismiss(text) {
    const element = this.getShadowElement_('dismiss');
    if (text) {
      element.setAttribute('aria-label', text);
      element.onclick = this.onclick;
    } else {
      element.removeAttribute('aria-label');  // hide
      element.onclick = null;
    }
  }

  /**
   * Sets action button text. Hides action button if |text| falsey.
   * @param {?string} text
   * @public
   */
  set action(text) {
    const element = this.getShadowElement_('action');
    if (text) {
      element.setAttribute('aria-label', text);
      element.onclick = this.onclick;
    } else {
      element.removeAttribute('aria-label');  // hide
      element.onclick = null;
    }
  }

  /**
   * Sets close icon type, aria-label. Hides close icon if |type| falsey.
   * @param {?{
   *   label:?string,
   * }} type
   * @public
   */
  set close(type) {
    const element = this.getShadowElement_('close');
    if (type) {
      element.setAttribute('aria-label', type.label || '');
      element.setAttribute('iron-icon', 'cr:close');
      element.onclick = this.onclick;
    } else {
      element.removeAttribute('iron-icon');  // hide
      element.onclick = null;
    }
  }
}

customElements.define('files-message', FilesMessage);
