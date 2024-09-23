// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '//resources/js/assert.js';
import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {FindShortcutManager} from './find_shortcut_manager.js';
import type {FindShortcutListener} from './find_shortcut_manager.js';

type Constructor<T> = new (...args: any[]) => T;

/**
 * Used to determine how to handle find shortcut invocations.
 */
export const FindShortcutMixinLit = <T extends Constructor<CrLitElement>>(
    superClass: T): T&Constructor<FindShortcutListener> => {
  class FindShortcutMixinLit extends superClass implements
      FindShortcutListener {
    findShortcutListenOnAttach: boolean = true;

    override connectedCallback() {
      super.connectedCallback();
      if (this.findShortcutListenOnAttach) {
        this.becomeActiveFindShortcutListener();
      }
    }

    override disconnectedCallback() {
      super.disconnectedCallback();
      if (this.findShortcutListenOnAttach) {
        this.removeSelfAsFindShortcutListener();
      }
    }

    becomeActiveFindShortcutListener() {
      const listeners = FindShortcutManager.listeners;
      assert(
          !listeners.includes(this), 'Already listening for find shortcuts.');
      listeners.push(this);
    }

    private handleFindShortcutInternal_() {
      assertNotReached('Must override handleFindShortcut()');
    }

    handleFindShortcut(_modalContextOpen: boolean) {
      this.handleFindShortcutInternal_();
      return false;
    }

    removeSelfAsFindShortcutListener() {
      const listeners = FindShortcutManager.listeners;
      const index = listeners.indexOf(this);
      assert(listeners.includes(this), 'Find shortcut listener not found.');
      listeners.splice(index, 1);
    }

    private searchInputHasFocusInternal_() {
      assertNotReached('Must override searchInputHasFocus()');
    }

    searchInputHasFocus() {
      this.searchInputHasFocusInternal_();
      return false;
    }
  }

  return FindShortcutMixinLit;
};
