// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {KeyboardShortcutList} from '//resources/js/keyboard_shortcut_list.js';
import {isMac} from '//resources/js/platform.js';

export interface FindShortcutListener {
  findShortcutListenOnAttach: boolean;
  becomeActiveFindShortcutListener(): void;

  /** If handled, return true. */
  handleFindShortcut(modalContextOpen: boolean): boolean;

  removeSelfAsFindShortcutListener(): void;
  searchInputHasFocus(): boolean;
}

/**
 * @fileoverview Listens for a find keyboard shortcut (i.e. Ctrl/Cmd+f or /)
 * and keeps track of an stack of potential listeners. Only the listener at the
 * top of the stack will be notified that a find shortcut has been invoked.
 */

export const FindShortcutManager = (() => {
  /**
   * Stack of listeners. Only the top listener will handle the shortcut.
   */
  const listeners: FindShortcutListener[] = [];

  /**
   * Tracks if any modal context is open in settings. This assumes only one
   * modal can be open at a time. The modals that are being tracked include
   * cr-dialog and cr-drawer.
   * @type {boolean}
   */
  let modalContextOpen = false;

  const shortcutCtrlF = new KeyboardShortcutList(isMac ? 'meta|f' : 'ctrl|f');
  const shortcutSlash = new KeyboardShortcutList('/');

  window.addEventListener('keydown', e => {
    if (e.defaultPrevented || listeners.length === 0) {
      return;
    }

    const element = e.composedPath()[0] as Element;
    if (!shortcutCtrlF.matchesEvent(e) &&
        (element.tagName === 'INPUT' || element.tagName === 'TEXTAREA' ||
         !shortcutSlash.matchesEvent(e))) {
      return;
    }

    const focusIndex =
        listeners.findIndex(listener => listener.searchInputHasFocus());
    // If no listener has focus or the first (outer-most) listener has focus,
    // try the last (inner-most) listener.
    // If a listener has a search input with focus, the next listener that
    // should be called is the right before it in |listeners| such that the
    // goes from inner-most to outer-most.
    const index = focusIndex <= 0 ? listeners.length - 1 : focusIndex - 1;
    if (listeners[index]!.handleFindShortcut(modalContextOpen)) {
      e.preventDefault();
    }
  });

  window.addEventListener('cr-dialog-open', () => {
    modalContextOpen = true;
  });

  window.addEventListener('cr-drawer-opened', () => {
    modalContextOpen = true;
  });

  window.addEventListener('close', e => {
    if (['CR-DIALOG', 'CR-DRAWER'].includes(
            (e.composedPath()[0] as Element).nodeName)) {
      modalContextOpen = false;
    }
  });

  return Object.freeze({listeners: listeners});
})();
