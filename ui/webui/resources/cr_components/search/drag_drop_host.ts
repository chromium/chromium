// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shared interface that connects parent element to the drag and drop handler.
export interface DragAndDropHost {
  isDraggingFile: boolean;
  animationState: string;

  /*
   * Returns the parent element that has ability
   * to receive dropped files. This method must be implemented in the
   * parent component that wishes to utilize the drag and drop handler.
   * animationState is to update searchAnimatedGlowElement's animation,
   * and isDraggingFile is a state that indicates if dragging is occurring.
   */
  getDropTarget(): {addFiles(files: FileList): void};
}
