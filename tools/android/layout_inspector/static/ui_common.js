// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** InfoBarVis ********/
/**
 * Manages the status bar shown at the bottom, displaying contextual hints.
 */
class InfoBarVis {
  constructor(divInfoBar) {
    this.el = {
      root: divInfoBar,
      infoHint: divInfoBar.querySelector('.hint-text'),
    };
  }

  setHintText(hintText) {
    this.el.infoHint.textContent = hintText;
  }
}

/******** Hint Constants ********/

const HINT = checkEnum({
  UNINITIALIZED: -1,
  IDLE: 0,
  CTRL_LOAD: 1,
  LAYOUT_SPLITTER: 2,
});

const HINT_STRINGS = {
  [HINT.IDLE]: '',
  [HINT.CTRL_LOAD]: 'Fetch UI hierarchy and screenshot from device.',
  [HINT.LAYOUT_SPLITTER]: 'Drag: Resize',
};

/******** HintController ********/
class HintController {
  constructor(infoBarVis) {
    this.infoBarVis = infoBarVis;
    this.currentHint = HINT.UNINITIALIZED;
    this.clear();  // Initialize default text.
  }

  setHint(hintEnum) {
    if (this.currentHint === hintEnum) return;
    this.currentHint = hintEnum;
    this.infoBarVis.setHintText(HINT_STRINGS[hintEnum]);
  }

  clear() {
    this.setHint(HINT.IDLE);
  }
}

/******** DragHandler ********/
/**
 * Generic `PointerEvent` interceptor ensuring robust capture/release state
 * logic for UI elements (like the pane splitter) rather than the screenshot
 * itself.
 */
class DragHandler {
  constructor(dragEl, {
    onDragStart,
    onDrag,
    onDragEnd,
    onDragCancel,
    pointerStyle = 'default',
  }) {
    this.dragEl = dragEl;
    this.onDragStart = onDragStart;
    this.onDrag = onDrag;
    this.onDragEnd = onDragEnd;
    this.onDragCancel = onDragCancel ?? onDragEnd;  // Fallback to onDragEnd.
    this.pointerStyle = pointerStyle;
    this.isDragging = false;
    this.bindAll();
  }

  setPointerStyle(style) {
    this.pointerStyle = style;
    if (!this.isDragging) {
      this.dragEl.style.cursor = style;
    }
  }

  handleDragStart(e) {
    e.preventDefault();

    // If this is the second click (or more) of a sequence, it's likely a
    // dblclick. Skip the drag logic to ensure the dblclick event fires on the
    // element.
    if (e.detail > 1) return;

    this.isDragging = true;
    const [startX, startY] = [e.clientX, e.clientY];
    if (this.onDragStart) this.onDragStart(e);

    document.body.style.setProperty('cursor', this.pointerStyle, 'important');
    this.dragEl.setPointerCapture(e.pointerId);

    const cleanup = (pointerId) => {
      this.isDragging = false;
      if (pointerId !== undefined) {
        this.dragEl.releasePointerCapture(pointerId);
      }
      this.dragEl.removeEventListener('pointermove', onPointerMove);
      this.dragEl.removeEventListener('pointerup', finish);
      this.dragEl.removeEventListener('pointercancel', cancel);
      document.removeEventListener('keydown', onKeyDown);
      document.body.style.removeProperty('cursor');
    };

    const onPointerMove = (moveEvent) => {
      if (moveEvent.buttons === 0) {
        finish(moveEvent);
        return;
      }
      if (this.onDrag) {
        const dx = moveEvent.clientX - startX;
        const dy = moveEvent.clientY - startY;
        this.onDrag(moveEvent, dx, dy);
      }
    };

    const finish = (upEvent) => {
      cleanup(upEvent.pointerId);
      if (this.onDragEnd) {
        this.onDragEnd(upEvent);
      }
    };

    const cancel = (cancelEvent) => {
      cleanup(cancelEvent?.pointerId);
      if (this.onDragCancel) {
        this.onDragCancel(cancelEvent);
      }
    };

    const onKeyDown = (keyEvent) => {
      if (keyEvent.key === 'Escape') {
        keyEvent.preventDefault();
        cancel(keyEvent);
      }
    };

    this.dragEl.addEventListener('pointermove', onPointerMove);
    this.dragEl.addEventListener('pointerup', finish);
    this.dragEl.addEventListener('pointercancel', cancel);
    document.addEventListener('keydown', onKeyDown);
  }

  bindAll() {
    this.dragEl.addEventListener('pointerdown', (e) => this.handleDragStart(e));
  }
}

/******** OverlayVis ********/
/**
 * Manages a global modal overlay for specialized UI states (Loading, Layout
 * Changes, Errors).
 */
class OverlayVis {
  constructor(divOverlay) {
    this.el = {
      root: divOverlay,
      content: divOverlay.querySelector('.content'),
    };
  }

  show(className, textContent = '') {
    this.el.root.className = className;
    this.el.root.classList.remove('hidden');
    this.el.content.textContent = textContent;
  }

  hide() {
    this.el.root.className = 'hidden';
  }
}
