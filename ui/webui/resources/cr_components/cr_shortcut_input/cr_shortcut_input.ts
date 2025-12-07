// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';

import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {isMac} from '//resources/js/platform.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_shortcut_input.css.js';
import {getHtml} from './cr_shortcut_input.html.js';
import {formatShortcutText, hasValidModifiers, isValidKeyCode, Key, keystrokeToString} from './cr_shortcut_util.js';

enum ShortcutError {
  NO_ERROR = 0,
  INCLUDE_START_MODIFIER = 1,
  TOO_MANY_MODIFIERS = 2,
  NEED_CHARACTER = 3,
}

// The UI to display and manage keyboard shortcuts.

export interface CrShortcutInputElement {
  $: {
    input: CrInputElement,
    edit: CrIconButtonElement,
  };
}

const CrShortcutInputElementBase = I18nMixinLit(CrLitElement);

export class CrShortcutInputElement extends CrShortcutInputElementBase {
  static get is() {
    return 'cr-shortcut-input';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      shortcut: {type: String},
      inputAriaLabel: {type: String},
      editButtonAriaLabel: {type: String},
      inputDisabled: {type: Boolean},
      allowCtrlAltShortcuts: {type: Boolean},
      error_: {type: Number},

      readonly_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor shortcut: string = '';
  accessor inputAriaLabel: string = '';
  accessor editButtonAriaLabel: string = '';
  accessor inputDisabled: boolean = false;
  accessor allowCtrlAltShortcuts = false;
  protected accessor readonly_: boolean = true;
  private capturing_: boolean = false;
  private accessor error_: ShortcutError = ShortcutError.NO_ERROR;
  private pendingShortcut_: string = '';

  override firstUpdated() {
    const node = this.$.input;
    node.addEventListener('mouseup', this.startCapture_.bind(this));
    node.addEventListener('blur', this.endCapture_.bind(this));
    node.addEventListener('focus', this.startCapture_.bind(this));
    node.addEventListener('keydown', this.onKeyDown_.bind(this));
    node.addEventListener('keyup', this.onKeyUp_.bind(this));
  }

  getBubbleAnchor() {
    return this.$.edit;
  }

  private async startCapture_() {
    if (this.capturing_ || this.readonly_) {
      return;
    }
    this.capturing_ = true;
    await this.updateComplete;
    this.fire('input-capture-change', true);
  }

  private async endCapture_() {
    if (!this.capturing_) {
      return;
    }
    this.pendingShortcut_ = '';
    this.capturing_ = false;
    this.$.input.blur();
    this.error_ = ShortcutError.NO_ERROR;
    this.readonly_ = true;
    await this.updateComplete;
    this.fire('input-capture-change', false);
  }

  private clearShortcut_() {
    this.pendingShortcut_ = '';
    this.shortcut = '';
    // Commit the empty shortcut in order to clear the current shortcut.
    this.commitPending_();
    this.endCapture_();
  }

  private onKeyDown_(e: KeyboardEvent) {
    if (this.readonly_) {
      return;
    }

    if (e.target === this.$.edit) {
      return;
    }

    if (e.keyCode === Key.ESCAPE) {
      if (!this.capturing_) {
        // If not currently capturing, allow escape to propagate.
        return;
      }
      // Otherwise, escape cancels capturing.
      this.endCapture_();
      e.preventDefault();
      e.stopPropagation();
      return;
    }
    if (e.keyCode === Key.TAB) {
      // Allow tab propagation for keyboard navigation.
      return;
    }

    if (!this.capturing_) {
      this.startCapture_();
    }

    this.handleKey_(e);
  }

  private onKeyUp_(e: KeyboardEvent) {
    // Ignores pressing 'Space' or 'Enter' on the edit button. In 'Enter's
    // case, the edit button disappears before key-up, so 'Enter's key-up
    // target becomes the input field, not the edit button, and needs to
    // be caught explicitly.
    if (this.readonly_) {
      return;
    }

    if (e.target === this.$.edit || e.key === 'Enter') {
      return;
    }

    if (e.keyCode === Key.ESCAPE || e.keyCode === Key.TAB) {
      return;
    }

    this.handleKey_(e);
  }

  protected getErrorString_(): string {
    switch (this.error_) {
      case ShortcutError.INCLUDE_START_MODIFIER:
        return this.i18n('shortcutIncludeStartModifier');
      case ShortcutError.TOO_MANY_MODIFIERS:
        return this.i18n('shortcutTooManyModifiers');
      case ShortcutError.NEED_CHARACTER:
        return this.i18n('shortcutNeedCharacter');
      default:
        assert(this.error_ === ShortcutError.NO_ERROR);
        return '';
    }
  }

  private handleKey_(e: KeyboardEvent) {
    // While capturing, prevent all events from bubbling, to prevent
    // shortcuts lacking the right modifier (F3 for example) from activating
    // and ending capture prematurely.
    e.preventDefault();
    e.stopPropagation();

    // Don't allow both Ctrl and Alt in the same keybinding. Profile saved
    // shortcuts convert command to Ctrl so command + alt is not allowed either.
    // See https://devblogs.microsoft.com/oldnewthing/20040329-00/?p=40003 for
    // more information.
    // TODO(devlin): This really should go in hasValidModifiers,
    // but that requires updating the existing page as well.
    if (!this.allowCtrlAltShortcuts && e.altKey &&
        (e.ctrlKey || (isMac && e.metaKey))) {
      this.error_ = ShortcutError.TOO_MANY_MODIFIERS;
      return;
    }
    // <if expr="is_macosx">
    // If Ctrl+Alt shortcuts are allowed, instances of too many modifiers
    // should still be blocked. This can only occur on Mac when all modifiers
    // are used.
    if (this.allowCtrlAltShortcuts && e.metaKey && e.altKey && e.shiftKey &&
        e.ctrlKey) {
      this.error_ = ShortcutError.TOO_MANY_MODIFIERS;
      return;
    }
    // </if>
    if (!hasValidModifiers(e)) {
      this.pendingShortcut_ = '';
      this.error_ = ShortcutError.INCLUDE_START_MODIFIER;
      return;
    }
    this.pendingShortcut_ = keystrokeToString(e);
    if (!isValidKeyCode(e.keyCode)) {
      this.error_ = ShortcutError.NEED_CHARACTER;
      return;
    }

    this.error_ = ShortcutError.NO_ERROR;

    getAnnouncerInstance().announce(
        this.i18n('shortcutSet', formatShortcutText(this.pendingShortcut_)));

    this.commitPending_();
    this.endCapture_();
  }

  private async commitPending_() {
    this.shortcut = this.pendingShortcut_;
    await this.updateComplete;
    this.fire('shortcut-updated', this.shortcut);
  }

  protected computePlaceholder_(): string {
    if (this.readonly_) {
      return this.shortcut ? this.i18n('shortcutSet', this.computeText_()) :
                             this.i18n('shortcutNotSet');
    }
    return this.i18n('shortcutTypeAShortcut');
  }

  /**
   * @return The text to be displayed in the shortcut field.
   */
  protected computeText_(): string {
    if (this.inputDisabled) {
      return this.i18n('setShortcutInSystemSettings');
    }
    return formatShortcutText(this.shortcut);
  }

  protected getIsInvalid_(): boolean {
    return this.error_ !== ShortcutError.NO_ERROR;
  }

  protected onEditClick_() {
    // TODO(ghazale): The clearing functionality should be improved.
    // Instead of clicking the edit button, and then clicking elsewhere to
    // commit the "empty" shortcut, introduce a separate clear button.
    this.clearShortcut_();
    this.readonly_ = false;
    this.$.input.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-shortcut-input': CrShortcutInputElement;
  }
}

customElements.define(CrShortcutInputElement.is, CrShortcutInputElement);
