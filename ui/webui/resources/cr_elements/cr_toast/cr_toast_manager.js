// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.toastManager', () => {
  /* eslint-disable */
  /** @private {?cr.toastManager.CrToastManagerElement} */
  let toastManagerInstance = null;
  /* eslint-enable */

  /** @return {!cr.toastManager.CrToastManagerElement} */
  /* #export */ function getInstance() {
    return assert(toastManagerInstance);
  }

  /** @param {?cr.toastManager.CrToastManagerElement} instance */
  /* #export */ function setInstance(instance) {
    assert(!instance || !toastManagerInstance);
    toastManagerInstance = instance;
  }

  /**
   * @fileoverview Element which shows toasts with optional undo button.
   */
  // eslint-disable-next-line
  /* #export */ let CrToastManagerElement = Polymer({
    is: 'cr-toast-manager',

    properties: {
      duration: {
        type: Number,
        value: 0,
      },

      /** @private */
      showUndo_: Boolean,

      undoDescription: String,

      undoLabel: String,
    },

    /** @return {boolean} */
    get isToastOpen() {
      return this.$.toast.open;
    },

    /** @return {boolean} */
    get isUndoButtonHidden() {
      return this.$.button.hidden;
    },

    /** @override */
    attached: function() {
      cr.toastManager.setInstance(this);
    },

    /** @override */
    detached: function() {
      cr.toastManager.setInstance(null);
    },

    /**
     * @param {string} label The label to display inside the toast.
     * @param {boolean} showUndo Whether the undo button should be shown.
     */
    show: function(label, showUndo) {
      this.$.content.textContent = label;
      this.showInternal_(showUndo);
      this.$.toast.show();
    },

    /**
     * Shows the toast, making certain text fragments collapsible.
     * @param {!Array<!{value: string, collapsible: boolean}>} pieces
     * @param {boolean} showUndo Whether the undo button should be shown.
     */
    showForStringPieces: function(pieces, showUndo) {
      const content = this.$.content;
      content.textContent = '';
      pieces.forEach(function(p) {
        if (p.value.length == 0) {
          return;
        }

        const span = document.createElement('span');
        span.textContent = p.value;
        if (p.collapsible) {
          span.classList.add('collapsible');
        }

        content.appendChild(span);
      });

      this.showInternal_(showUndo);
    },

    /**
     * @param {boolean} showUndo Whether the undo button should be shown.
     * @private
     */
    showInternal_: function(showUndo) {
      this.showUndo_ = showUndo;
      Polymer.IronA11yAnnouncer.requestAvailability();
      this.fire('iron-announce', {
        text: this.$.content.textContent,
      });
      if (showUndo && this.undoDescription) {
        this.fire('iron-announce', {
          text: this.undoDescription,
        });
      }
      this.$.toast.show();
    },

    hide: function() {
      this.$.toast.hide();
    },

    /** @private */
    onUndoClick_: function() {
      this.fire('undo-click');
    },
  });

  // #cr_define_end
  return {
    CrToastManagerElement: CrToastManagerElement,
    getInstance: getInstance,
    setInstance: setInstance,
  };
});
