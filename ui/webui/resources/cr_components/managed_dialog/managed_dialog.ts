// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'managed-dialog' is a dialog that is displayed when a user
 * interact with some UI features which are managed by the user's organization.
 */
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons_lit.html.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './managed_dialog.css.js';
import {getHtml} from './managed_dialog.html.js';

export interface ManagedDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const ManagedDialogElementBase = I18nMixinLit(CrLitElement);

export class ManagedDialogElement extends ManagedDialogElementBase {
  static get is() {
    return 'managed-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /** Managed dialog title text. */
      title: {type: String},

      /** Managed dialog body text. */
      body: {type: String},
    };
  }

  override title: string = '';
  body: string = '';

  protected onOkClick_() {
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'managed-dialog': ManagedDialogElement;
  }
}

customElements.define(ManagedDialogElement.is, ManagedDialogElement);
