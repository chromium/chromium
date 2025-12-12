// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GlowAnimationState} from './constants.js';
import type {DragAndDropHost} from './drag_drop_host.js';

/*
 * To use, make parent class of interest implement DragDropHost.
 * Host needs a file entrypoint to submit file in its implementation of
 * 'addDroppedFiles()'. After, add handler as attribute of parent class. Utilize
 * event methods from this class as event listeners in parent class.
 */
export class DragAndDropHandler {
  private host_: DragAndDropHost;
  private dragAndDropEnabled_: boolean = false;
  /*
   * Since there are many dragEnter events, keep track of each and
   * once there are same number of dragLeaves as dragEnters, all dragEnter
   * events have ended, and thus drag state should end fully.
   */
  private enterCounter_: number = 0;

  constructor(host: DragAndDropHost, dragAndDropEnabled: boolean) {
    this.host_ = host;
    this.dragAndDropEnabled_ = dragAndDropEnabled;
    this.handleDragEnter = this.handleDragEnter.bind(this);
    this.handleDragLeave = this.handleDragLeave.bind(this);
    this.handleDragOver = this.handleDragOver.bind(this);
    this.handleDrop = this.handleDrop.bind(this);
  }

  handleDragEnter(e: DragEvent) {
    if (!this.dragAndDropEnabled_) {
      return;
    }
    e.preventDefault();

    this.enterCounter_++;

    // First dragEnter initiates dragging state.
    if (this.enterCounter_ === 1) {
      this.host_.isDraggingFile = true;
      this.host_.animationState = GlowAnimationState.DRAGGING;
    }
  }

  handleDragOver(e: DragEvent) {
    if (!this.dragAndDropEnabled_) {
      return;
    }
    e.preventDefault();
  }

  handleDrop(e: DragEvent) {
    if (!this.dragAndDropEnabled_) {
      return;
    }
    e.preventDefault();

    this.enterCounter_ = 0;

    const files = e.dataTransfer?.files;
    if (files && files.length > 0) {
      this.host_.getDropTarget().addDroppedFiles(files);
    }
    this.host_.isDraggingFile = false;
    this.host_.animationState = GlowAnimationState.NONE;
  }

  handleDragLeave(e: DragEvent) {
    if (!this.dragAndDropEnabled_) {
      return;
    }
    e.preventDefault();

    this.enterCounter_--;

    // Last dragEnter has left, meaning drag has ended.
    if (this.enterCounter_ === 0) {
      this.host_.isDraggingFile = false;
      this.host_.animationState = GlowAnimationState.NONE;
    }
  }
}
