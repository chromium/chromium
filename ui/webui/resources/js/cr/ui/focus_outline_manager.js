// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {
  /**
   * The class name to set on the document element.
   * @const
   */
  const CLASS_NAME = 'focus-outline-visible';

  /** @type {!Map<!Document, !cr.ui.FocusOutlineManager>} */
  const docsToManager = new Map();

  /**
   * This class sets a CSS class name on the HTML element of |doc| when the user
   * presses a key. It removes the class name when the user clicks anywhere.
   *
   * This allows you to write CSS like this:
   *
   * html.focus-outline-visible my-element:focus {
   *   outline: 5px auto -webkit-focus-ring-color;
   * }
   *
   * And the outline will only be shown if the user uses the keyboard to get to
   * it.
   *
   */
  /* #export */ class FocusOutlineManager {
    /**
     * @param {!Document} doc The document to attach the focus outline manager
     *     to.
     */
    constructor(doc) {
      /**
       * Whether focus change is triggered by a keyboard event.
       * @private {boolean}
       */
      this.focusByKeyboard_ = true;

      this.classList_ = doc.documentElement.classList;

      const onEvent = function(focusByKeyboard, e) {
        if (this.focusByKeyboard_ === focusByKeyboard) {
          return;
        }
        this.focusByKeyboard_ = focusByKeyboard;
        this.updateVisibility();
      };

      doc.addEventListener('keydown', onEvent.bind(this, true), true);
      doc.addEventListener('mousedown', onEvent.bind(this, false), true);

      this.updateVisibility();
    }

    updateVisibility() {
      this.visible = this.focusByKeyboard_;
    }

    /**
     * Whether the focus outline should be visible.
     * @type {boolean}
     */
    set visible(visible) {
      this.classList_.toggle(CLASS_NAME, visible);
    }

    get visible() {
      return this.classList_.contains(CLASS_NAME);
    }

    /**
     * Gets a per document singleton focus outline manager.
     * @param {!Document} doc The document to get the |FocusOutlineManager| for.
     * @return {!cr.ui.FocusOutlineManager} The per document singleton focus
     *     outline manager.
     */
    static forDocument(doc) {
      let manager = docsToManager.get(doc);
      if (!manager) {
        manager = new FocusOutlineManager(doc);
        docsToManager.set(doc, manager);
      }
      return manager;
    }
  }

  // #cr_define_end
  return {FocusOutlineManager: FocusOutlineManager};
});
