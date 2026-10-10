// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {assertNotReachedCase} from '//resources/js/assert.js';
import {isMac} from '//resources/js/platform.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './composebox_aim_button.css.js';
import {getHtml} from './composebox_aim_button.html.js';

export enum AimButtonMode {
  EXIT = 'exit',
  SEND = 'send',
}

// The string is used as the CSS class of the trailing icon element (close icon
// or arrow icon). Keep the string in sync with the selectors in the stylesheet.
enum TrailingIcon {
  NONE = '',
  ARROW = 'arrow-icon',
  EXIT = 'icon-clear',
}

export interface ComposeboxAimButtonElement {
  $: {
    button: CrButtonElement,
  };
}

export class ComposeboxAimButtonElement extends CrLitElement {
  static get is() {
    return 'cr-composebox-aim-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      mode: {type: String},
      showExitIcon: {type: Boolean},
      disabled: {type: Boolean, reflect: true},
      label: {type: String},
      leadingIconUrl: {type: String},
      exitTitle: {type: String},
      sendTitle: {type: String},
    };
  }

  // `mode` is not reflected; styling is conditioned on icon presence rather
  // than the mode state itself.
  accessor mode: AimButtonMode = AimButtonMode.EXIT;
  accessor showExitIcon: boolean = false;
  accessor disabled: boolean = false;
  accessor label: string = '';
  accessor leadingIconUrl: string = '';
  accessor exitTitle: string = '';
  accessor sendTitle: string = '';

  private keyboardActivationEvent_: KeyboardEvent|null = null;

  constructor() {
    super();

    // cr-button calls click() on Enter/Space, and the synthesized click
    // carries no modifier keys. Capture the keyboard event so
    // `getActivationEvent_()` can forward it instead.
    const onKeyboardActivation = this.onKeyboardActivation_.bind(this);
    this.addEventListener('keydown', onKeyboardActivation, {capture: true});
    this.addEventListener('keyup', onKeyboardActivation, {capture: true});
  }

  override focus() {
    this.$.button?.focus();
  }

  override blur() {
    this.$.button?.blur();
  }

  // Only the exit state without X (exit icon) shows the leading icon.
  protected showLeadingIcon_(): boolean {
    return this.mode === AimButtonMode.EXIT && !this.showExitIcon &&
        !!this.leadingIconUrl;
  }

  // Send state shows the arrow icon; exit state with X (exit icon) shows the
  // exit icon; exit state without X doesn't show any trailing icon.
  protected getTrailingIcon_(): TrailingIcon {
    switch (this.mode) {
      case AimButtonMode.SEND:
        return TrailingIcon.ARROW;
      case AimButtonMode.EXIT:
        return this.showExitIcon ? TrailingIcon.EXIT : TrailingIcon.NONE;
      default:
        assertNotReachedCase(this.mode);
    }
  }

  protected getTitle_(): string {
    switch (this.mode) {
      case AimButtonMode.EXIT:
        return this.exitTitle;
      case AimButtonMode.SEND:
        return this.sendTitle;
      default:
        assertNotReachedCase(this.mode);
    }
  }

  protected onClick_(e: MouseEvent) {
    e.preventDefault();
    if (this.disabled) {
      return;
    }

    // The label (text) and X (exit icon) share this single click entry point.
    this.fire('aim-button-click', this.getActivationEvent_(e));
  }

  private onKeyboardActivation_(e: KeyboardEvent) {
    // Clear the cache on every key event so stale modifiers never linger.
    this.keyboardActivationEvent_ = null;
    if (this.disabled) {
      return;
    }

    // Same conditions under which cr-button triggers a click(): Enter on
    // keydown (excluding repeats), or Space on keyup.
    const keyDownActivated = e.type === 'keydown' && e.key === 'Enter' &&
        !e.repeat && !(isMac && e.ctrlKey);
    const keyUpActivated = e.type === 'keyup' && e.key === ' ';
    if ((keyDownActivated || keyUpActivated) &&
        e.composedPath()[0] === this.$.button) {
      this.keyboardActivationEvent_ = e;
    }
  }

  private getActivationEvent_(e: MouseEvent): KeyboardEvent|MouseEvent {
    const keyboardEvent = this.keyboardActivationEvent_;
    this.keyboardActivationEvent_ = null;
    // Use the keyboard event only when the click was synthesized from the
    // keyboard (detail === 0) and the cached event is still being dispatched
    // on the same button. Otherwise the cache is stale, so use the click
    // event itself.
    if (e.detail === 0 && keyboardEvent &&
        keyboardEvent.eventPhase === Event.AT_TARGET &&
        keyboardEvent.currentTarget === e.currentTarget) {
      return keyboardEvent;
    }
    return e;
  }
}

customElements.define(
    ComposeboxAimButtonElement.is, ComposeboxAimButtonElement);

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-aim-button': ComposeboxAimButtonElement;
  }
}
