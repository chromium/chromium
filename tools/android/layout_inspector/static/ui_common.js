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
});

const HINT_STRINGS = {
  [HINT.IDLE]: '',
  [HINT.CTRL_LOAD]: 'Fetch UI hierarchy and screenshot from device.',
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
