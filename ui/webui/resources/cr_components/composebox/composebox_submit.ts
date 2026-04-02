// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assertNotReachedCase} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {SubmitButtonIconType} from './composebox.js';
import {getCss} from './composebox_submit.css.js';
import {getHtml} from './composebox_submit.html.js';

const ComposeboxSubmitElementBase = I18nMixinLit(CrLitElement);

export interface ComposeboxSubmitElement {
  $: {
    submitContainer: HTMLElement,
    submitIcon: CrIconButtonElement,
  };
}

export class ComposeboxSubmitElement extends ComposeboxSubmitElementBase {
  static get is() {
    return 'cr-composebox-submit';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {type: Boolean, reflect: true},
      iconType: {type: String},
      submitButtonTitle: {type: String},
    };
  }

  accessor disabled: boolean = false;
  accessor iconType: SubmitButtonIconType = SubmitButtonIconType.UPWARD;
  accessor submitButtonTitle: string = '';

  protected submitButtonIconClass_(): string {
    switch (this.iconType) {
      case SubmitButtonIconType.FORWARD:
        return 'icon-arrow-forward';
      case SubmitButtonIconType.UPWARD:
        return 'icon-arrow-upward';
      default:
        assertNotReachedCase(this.iconType);
    }
  }

  protected onSubmitClick_(e: MouseEvent) {
    if (!this.disabled) {
      this.fire('submit-click', e);
    }
  }

  protected onSubmitFocusin_(e: FocusEvent) {
    this.fire('submit-focusin', e);
  }
}

customElements.define(ComposeboxSubmitElement.is, ComposeboxSubmitElement);

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-submit': ComposeboxSubmitElement;
  }
}
