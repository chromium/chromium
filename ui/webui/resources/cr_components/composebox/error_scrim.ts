// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './error_scrim.css.js';
import {getHtml} from './error_scrim.html.js';

export class ErrorScrimElement extends I18nMixinLit
(CrLitElement) {

  static get is() {
    return 'ntp-error-scrim';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      compactMode: {
        type: Boolean,
      },
      showErrorScrim_: {
        reflect: true,
        type: Boolean,
      },
      errorMessage_: {
        type: String,
      },
    };
  }

  accessor compactMode: boolean = false;
  protected accessor showErrorScrim_: boolean = false;
  protected accessor errorMessage_: string = '';

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('showErrorScrim_')) {
      if (this.showErrorScrim_) {
        const announcer = getAnnouncerInstance();
        announcer.announce(this.errorMessage_);
        const dismissErrorButton =
            this.shadowRoot.querySelector<HTMLElement>('#dismissErrorButton');
        if (dismissErrorButton) {
          dismissErrorButton.focus();
        }
      }
      this.fire('error-scrim-visibility-changed', {
        showErrorScrim: this.showErrorScrim_,
      });
    }
  }

  setErrorMessage(errorMessage: string) {
    this.errorMessage_ = errorMessage;
    this.showErrorScrim_ = true;
  }

  isErrorScrimShowing(): boolean {
    return this.showErrorScrim_;
  }

  protected onDismissErrorButtonClick_() {
    this.errorMessage_ = '';
    this.showErrorScrim_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-error-scrim': ErrorScrimElement;
  }
}

customElements.define(ErrorScrimElement.is, ErrorScrimElement);
