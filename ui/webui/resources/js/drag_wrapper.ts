// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview DragWrapper
 * A class for simplifying HTML5 drag and drop. Classes should use this to
 * handle the details of nested drag enters and leaves.
 */
export interface DragWrapperDelegate {
  // TODO(devlin): The only method this "delegate" actually needs is
  // shouldAcceptDrag(); the rest can be events emitted by the DragWrapper.
  /**
   * @return Whether the drag should be accepted. If false,
   *     subsequent methods (doDrag*) will not be called.
   */
  shouldAcceptDrag(e: MouseEvent): boolean;

  doDragEnter(e: MouseEvent): void;

  doDragLeave(e: MouseEvent): void;

  doDragOver(e: MouseEvent): void;

  doDrop(e: MouseEvent): void;
}

/**
 * Creates a DragWrapper which listens for drag target events on |target| and
 * delegates event handling to |delegate|.
 */
export class DragWrapper {
  /**
   * The number of un-paired dragenter events that have fired on |this|.
   * This is incremented by |onDragEnter_| and decremented by
   * |onDragLeave_|. This is necessary because dragging over child widgets
   * will fire additional enter and leave events on |this|. A non-zero value
   * does not necessarily indicate that |isCurrentDragTarget()| is true.
   */
  private dragEnters_: number = 0;
  private target_: HTMLElement;
  private delegate_: DragWrapperDelegate;

  constructor(target: HTMLElement, delegate: DragWrapperDelegate) {
    this.target_ = target;
    this.delegate_ = delegate;

    target.addEventListener('dragenter', e => this.onDragEnter_(e));
    target.addEventListener('dragover', e => this.onDragOver_(e));
    target.addEventListener('drop', e => this.onDrop_(e));
    target.addEventListener('dragleave', e => this.onDragLeave_(e));
  }

  /**
   * Whether the tile page is currently being dragged over with data it can
   * accept.
   */
  get isCurrentDragTarget(): boolean {
    return this.target_.classList.contains('drag-target');
  }

  /**
   * Delegate for dragenter events fired on |target_|.
   */
  private onDragEnter_(e: MouseEvent) {
    if (++this.dragEnters_ === 1) {
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
   */
  private onDragOver_(e: MouseEvent) {
    if (!this.target_.classList.contains('drag-target')) {
      return;
    }
    this.delegate_.doDragOver(e);
  }

  /**
   * Thunk for drop events fired on |target_|.
   */
  private onDrop_(e: MouseEvent) {
    this.dragEnters_ = 0;
    if (!this.target_.classList.contains('drag-target')) {
      return;
    }
    this.target_.classList.remove('drag-target');
    this.delegate_.doDrop(e);
  }

  /**
   * Thunk for dragleave events fired on |target_|.
   */
  private onDragLeave_(e: MouseEvent) {
    if (--this.dragEnters_ > 0) {
      return;
    }

    this.target_.classList.remove('drag-target');
    this.delegate_.doDragLeave(e);
  }
}
