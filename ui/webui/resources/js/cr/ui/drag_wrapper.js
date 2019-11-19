// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview DragWrapper
 * A class for simplifying HTML5 drag and drop. Classes should use this to
 * handle the nitty gritty of nested drag enters and leaves.
 */
cr.define('cr.ui', function() {
  /** @interface */
  /* #export */ class DragWrapperDelegate {
    // TODO(devlin): The only method this "delegate" actually needs is
    // shouldAcceptDrag(); the rest can be events emitted by the DragWrapper.
    /**
     * @param {MouseEvent} e The event for the drag.
     * @return {boolean} Whether the drag should be accepted. If false,
     *     subsequent methods (doDrag*) will not be called.
     */
    shouldAcceptDrag(e) {}

    /** @param {MouseEvent} e */
    doDragEnter(e) {}

    /** @param {MouseEvent} e */
    doDragLeave(e) {}

    /** @param {MouseEvent} e */
    doDragOver(e) {}

    /** @param {MouseEvent} e */
    doDrop(e) {}
  }

  /**
   * Creates a DragWrapper which listens for drag target events on |target| and
   * delegates event handling to |delegate|.
   */
  /* #export */ class DragWrapper {
    /**
     * @param {!Element} target
     * @param {!cr.ui.DragWrapperDelegate} delegate
     */
    constructor(target, delegate) {
      /**
       * The number of un-paired dragenter events that have fired on |this|.
       * This is incremented by |onDragEnter_| and decremented by
       * |onDragLeave_|. This is necessary because dragging over child widgets
       * will fire additional enter and leave events on |this|. A non-zero value
       * does not necessarily indicate that |isCurrentDragTarget()| is true.
       * @private {number}
       */
      this.dragEnters_ = 0;

      /** @private {!Element} */
      this.target_ = target;

      /** @private {!cr.ui.DragWrapperDelegate} */
      this.delegate_ = delegate;

      target.addEventListener(
          'dragenter', e => this.onDragEnter_(/** @type {!MouseEvent} */ (e)));
      target.addEventListener(
          'dragover', e => this.onDragOver_(/** @type {!MouseEvent} */ (e)));
      target.addEventListener(
          'drop', e => this.onDrop_(/** @type {!MouseEvent} */ (e)));
      target.addEventListener(
          'dragleave', e => this.onDragLeave_(/** @type {!MouseEvent} */ (e)));
    }

    /**
     * Whether the tile page is currently being dragged over with data it can
     * accept.
     * @return {boolean}
     */
    get isCurrentDragTarget() {
      return this.target_.classList.contains('drag-target');
    }

    /**
     * Delegate for dragenter events fired on |target_|.
     * @param {!MouseEvent} e A MouseEvent for the drag.
     * @private
     */
    onDragEnter_(e) {
      if (++this.dragEnters_ == 1) {
        if (this.delegate_.shouldAcceptDrag(e)) {
          this.target_.classList.add('drag-target');
          this.delegate_.doDragEnter(e);
        }
      } else {
        // Sometimes we'll get an enter event over a child element without an
        // over event following it. In this case we have to still call the
        // drag over delegate so that we make the necessary updates (one visible
        // symptom of not doing this is that the cursor's drag state will
        // flicker during drags).
        this.onDragOver_(e);
      }
    }

    /**
     * Thunk for dragover events fired on |target_|.
     * @param {!MouseEvent} e A MouseEvent for the drag.
     * @private
     */
    onDragOver_(e) {
      if (!this.target_.classList.contains('drag-target')) {
        return;
      }
      this.delegate_.doDragOver(e);
    }

    /**
     * Thunk for drop events fired on |target_|.
     * @param {!MouseEvent} e A MouseEvent for the drag.
     * @private
     */
    onDrop_(e) {
      this.dragEnters_ = 0;
      if (!this.target_.classList.contains('drag-target')) {
        return;
      }
      this.target_.classList.remove('drag-target');
      this.delegate_.doDrop(e);
    }

    /**
     * Thunk for dragleave events fired on |target_|.
     * @param {!MouseEvent} e A MouseEvent for the drag.
     * @private
     */
    onDragLeave_(e) {
      if (--this.dragEnters_ > 0) {
        return;
      }

      this.target_.classList.remove('drag-target');
      this.delegate_.doDragLeave(e);
    }
  }

  // #cr_define_end
  return {
    DragWrapper: DragWrapper,
    DragWrapperDelegate: DragWrapperDelegate,
  };
});
