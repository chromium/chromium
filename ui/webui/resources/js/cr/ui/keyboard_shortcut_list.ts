// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** This is used to identify keyboard shortcuts. */
class KeyboardShortcut {
  private useKeyCode_: boolean = false;
  private mods_: {[key: string]: boolean} = {};
  private key_: string|null = null;
  private keyCode_: number|null = null;

  /**
   * @param shortcut The text used to describe the keys for this
   *     keyboard shortcut.
   */
  constructor(shortcut: string) {
    shortcut.split('|').forEach((part) => {
      const partLc = part.toLowerCase();
      switch (partLc) {
        case 'alt':
        case 'ctrl':
        case 'meta':
        case 'shift':
          this.mods_[partLc + 'Key'] = true;
          break;
        default:
          if (this.key_) {
            throw Error('Invalid shortcut');
          }
          this.key_ = part;
          // For single key alpha shortcuts use event.keyCode rather than
          // event.key to match how chrome handles shortcuts and allow
          // non-english language input to work.
          if (part.match(/^[a-z]$/)) {
            this.useKeyCode_ = true;
            this.keyCode_ = part.toUpperCase().charCodeAt(0);
          }
      }
    });
  }

  /**
   * Whether the keyboard shortcut object matches a keyboard event.
   * @param e The keyboard event object.
   * @return Whether we found a match or not.
   */
  matchesEvent(e: KeyboardEvent): boolean {
    if ((this.useKeyCode_ && e.keyCode === this.keyCode_) ||
        e.key === this.key_) {
      // All keyboard modifiers need to match.
      const mods = this.mods_;
      return ['altKey', 'ctrlKey', 'metaKey', 'shiftKey'].every(function(k) {
        return (e as unknown as {[key: string]: boolean})[k] === !!mods[k];
      });
    }
    return false;
  }
}

/** A list of keyboard shortcuts which all perform one command. */
export class KeyboardShortcutList {
  private shortcuts_: KeyboardShortcut[];

  /**
   * @param shortcuts Text-based representation of one or more
   *     keyboard shortcuts, separated by spaces.
   */
  constructor(shortcuts: string) {
    this.shortcuts_ = shortcuts.split(/\s+/).map(function(shortcut) {
      return new KeyboardShortcut(shortcut);
    });
  }

  /**
   * Returns true if any of the keyboard shortcuts in the list matches a
   * keyboard event.
   */
  matchesEvent(e: KeyboardEvent): boolean {
    return this.shortcuts_.some(function(keyboardShortcut) {
      return keyboardShortcut.matchesEvent(e);
    });
  }
}

