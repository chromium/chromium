// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.toastManager', () => {
  /* eslint-disable */
  /** @private {?CrToastManagerElement} */
  let toastManagerInstance = null;
  /* eslint-enable */

  /** @return {!CrToastManagerElement} */
  /* #export */ function getToastManager() {
    return assert(toastManagerInstance);
  }

  /** @param {?CrToastManagerElement} instance */
  function setInstance(instance) {
    assert(!instance || !toastManagerInstance);
    toastManagerInstance = instance;
  }

  /**
   * @fileoverview Element which shows toasts with optional undo button.
   */
  // eslint-disable-next-line
  Polymer({
    is: 'cr-toast-manager',

    properties: {
      duration: {
        type: Number,
        value: 0,
      },
    },

    /** @return {boolean} */
    get isToastOpen() {
      return this.$.toast.open;
    },

    /** @return {boolean} */
    get slottedHidden() {
      return this.$.slotted.hidden;
    },

    /** @override */
    attached() {
      setInstance(this);
    },

    /** @override */
    detached() {
      setInstance(null);
    },

    /**
     * @param {string} label The label to display inside the toast.
     * @param {boolean=} hideSlotted
     */
    show(label, hideSlotted = false) {
      this.$.content.textContent = label;
      this.showInternal_(hideSlotted);
    },

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
    },

    /**
     * @param {boolean} hideSlotted
     * @private
     */
    showInternal_(hideSlotted) {
      this.$.slotted.hidden = hideSlotted;
      this.$.toast.show();
    },

    hide() {
      this.$.toast.hide();
    },
  });

  // #cr_define_end
  console.warn('crbug/1173575, non-JS module files deprecated.');
  return {
    getToastManager: getToastManager,
  };
});
