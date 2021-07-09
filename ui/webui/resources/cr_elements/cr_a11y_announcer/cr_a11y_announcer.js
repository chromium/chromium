// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * The CrA11yAnnouncerElement is a visually hidden element that reads out
 * messages to a screen reader. This is preferred over IronA11yAnnouncer.
 * @fileoverview
 */

/**
 * 150ms seems to be around the minimum time required for screen readers to
 * read out consecutively queued messages.
 * @type {number}
 */
export const TIMEOUT_MS = 150;

/**
 * A map of an HTML element to its corresponding CrA11yAnnouncerElement. There
 * may be multiple CrA11yAnnouncerElements on a page, especially for cases in
 * which the DocumentElement's CrA11yAnnouncerElement becomes hidden or
 * deactivated (eg. when a modal dialog causes the CrA11yAnnouncerElement to
 * become inaccessible).
 * @type {!Map<!HTMLElement, !CrA11yAnnouncerElement>}
 */
const instances = new Map();

export class CrA11yAnnouncerElement extends PolymerElement {
  static get is() {
    return 'cr-a11y-announcer';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  constructor() {
    super();

    /** @private {?number} */
    this.currentTimeout_ = null;

    /** @private {!Array<string>} */
    this.messages_ = [];
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    if (this.currentTimeout_ !== null) {
      clearTimeout(this.currentTimeout_);
      this.currentTimeout_ = null;
    }

    for (const [parent, instance] of instances) {
      if (instance === this) {
        instances.delete(parent);
        break;
      }
    }
  }

  /** @param {string} message */
  announce(message) {
    if (this.currentTimeout_ !== null) {
      clearTimeout(this.currentTimeout_);
      this.currentTimeout_ = null;
    }

    this.messages_.push(message);

    this.currentTimeout_ = setTimeout(() => {
      const messagesDiv = this.shadowRoot.querySelector('#messages');
      messagesDiv.innerHTML = '';

      // <if expr="is_macosx">
      // VoiceOver on Mac does not seem to consistently read out the contents of
      // a static alert element. Toggling the role of alert seems to force VO
      // to consistently read out the messages.
      messagesDiv.removeAttribute('role');
      messagesDiv.setAttribute('role', 'alert');
      // </if>

      for (const message of this.messages_) {
        const div = document.createElement('div');
        div.textContent = message;
        messagesDiv.appendChild(div);
      }

      this.messages_.length = 0;
      this.currentTimeout_ = null;
    }, TIMEOUT_MS);
  }

  /**
   * @param {!HTMLElement=} container
   * @return {!CrA11yAnnouncerElement}
   */
  static getInstance(container = document.body) {
    if (instances.has(container)) {
      return instances.get(container);
    }
    assert(container.isConnected);
    const instance = new CrA11yAnnouncerElement();
    container.appendChild(instance);
    instances.set(container, instance);
    return instance;
  }
}

customElements.define(CrA11yAnnouncerElement.is, CrA11yAnnouncerElement);
