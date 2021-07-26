// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Element which shows toasts with optional undo button. */

import '../../js/cr.m.js';
import '../../js/event_tracker.m.js';
import '../hidden_style_css.m.js';
import './cr_toast.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assert} from '../../js/assert.m.js';

/** @private {?CrToastManagerElement} */
let toastManagerInstance = null;

/** @return {!CrToastManagerElement} */
export function getToastManager() {
  return assert(toastManagerInstance);
}

/** @param {?CrToastManagerElement} instance */
function setInstance(instance) {
  assert(!instance || !toastManagerInstance);
  toastManagerInstance = instance;
}

/** @polymer */
export class CrToastManagerElement extends PolymerElement {
  static get is() {
    return 'cr-toast-manager';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      duration: {
        type: Number,
        value: 0,
      },
    };
  }

  /** @return {boolean} */
  get isToastOpen() {
    return this.$.toast.open;
  }

  /** @return {boolean} */
  get slottedHidden() {
    return this.$.slotted.hidden;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    setInstance(this);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();

    setInstance(null);
  }

  /**
   * @param {string} label The label to display inside the toast.
   * @param {boolean=} hideSlotted
   */
  show(label, hideSlotted = false) {
    this.$.content.textContent = label;
    this.showInternal_(hideSlotted);
  }

  /**
   * Shows the toast, making certain text fragments collapsible.
   * @param {!Array<!{value: string, collapsible: boolean}>} pieces
   * @param {boolean=} hideSlotted
   */
  showForStringPieces(pieces, hideSlotted = false) {
    const content = this.$.content;
    content.textContent = '';
    pieces.forEach(function(p) {
      if (p.value.length === 0) {
        return;
      }

      const span = document.createElement('span');
      span.textContent = p.value;
      if (p.collapsible) {
        span.classList.add('collapsible');
      }

      content.appendChild(span);
    });

    this.showInternal_(hideSlotted);
  }

  /**
   * @param {boolean} hideSlotted
   * @private
   */
  showInternal_(hideSlotted) {
    this.$.slotted.hidden = hideSlotted;
    this.$.toast.show();
  }

  hide() {
    this.$.toast.hide();
  }
}

customElements.define(CrToastManagerElement.is, CrToastManagerElement);
