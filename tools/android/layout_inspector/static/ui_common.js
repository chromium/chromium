// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** InfoBarVis ********/
/**
 * Manages the status bar shown at the bottom, displaying View Info and
 * contextual hints.
 */
class InfoBarVis {
  /** @param {!Element} divInfoBar */
  constructor(divInfoBar) {
    this.el = {
      root: divInfoBar,
      viewInfo: divInfoBar.querySelector('.view-info'),
      viewClass: divInfoBar.querySelector('.view-class'),
      sizePx: divInfoBar.querySelector('.size-px'),
      sizeDp: divInfoBar.querySelector('.size-dp'),
      infoHint: divInfoBar.querySelector('.hint-text'),
    };
  }

  renderAndShowViewInfo(view, visOpts) {
    this.el.viewClass.textContent = view.className;
    const wPx = view.rect.width();
    const hPx = view.rect.height();
    this.el.sizePx.textContent = `${wPx}x${hPx}px`;
    const wDp = capFixed(visOpts.pxToDp(wPx), 2);
    const hDp = capFixed(visOpts.pxToDp(hPx), 2);
    this.el.sizeDp.textContent = `${wDp}x${hDp}dp`;
    this.el.viewInfo.classList.remove('hidden');
  }

  hideViewInfo() {
    this.el.viewInfo.classList.add('hidden');
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
  CTRL_ZOOM: 2,
  LAYOUT_SPLITTER: 3,
});

const HINT_STRINGS = {
  [HINT.IDLE]: 'Hover over a View or control to see info.',
  [HINT.CTRL_LOAD]: 'Fetch UI hierarchy and screenshot from device.',
  [HINT.CTRL_ZOOM]: 'Change display scale of the screenshot.',
  [HINT.LAYOUT_SPLITTER]: 'Drag: Resize | Double-Click: Auto-Resize',
};

/******** HintController ********/
class HintController {
  /** @param {!InfoBarVis} infoBarVis */
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
  /**
   * @param {!Element} dragEl
   * @param {!Object} callbacks
   * @param {function(!PointerEvent)=} callbacks.onDragStart
   * @param {function(!PointerEvent, number, number)=} callbacks.onDrag
   * @param {function(!PointerEvent)=} callbacks.onDragEnd
   * @param {function(!(PointerEvent|KeyboardEvent))=} callbacks.onDragCancel
   * @param {function(!MouseEvent)=} callbacks.onDoubleClick
   * @param {string=} callbacks.pointerStyle
   */
  constructor(dragEl, {
    onDragStart,
    onDrag,
    onDragEnd,
    onDragCancel,
    onDoubleClick,
    pointerStyle = 'default',
  }) {
    this.dragEl = dragEl;
    this.onDragStart = onDragStart;
    this.onDrag = onDrag;
    this.onDragEnd = onDragEnd;
    this.onDragCancel = onDragCancel ?? onDragEnd;  // Fallback to onDragEnd.
    this.onDoubleClick = onDoubleClick;
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
    if (this.onDoubleClick) {
      this.dragEl.addEventListener('dblclick', (e) => this.onDoubleClick(e));
    }
  }
}

/******** OverlayVis ********/
/**
 * Manages a global modal overlay for specialized UI states (Loading, Layout
 * Changes, Errors).
 */
class OverlayVis {
  /** @param {!Element} divOverlay */
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
