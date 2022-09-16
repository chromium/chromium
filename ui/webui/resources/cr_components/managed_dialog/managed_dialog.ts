// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'managed-dialog' is a dialog that is displayed when a user
 * interact with some UI features which are managed by the user's organization.
 */
import '../../cr_elements/cr_button/cr_button.js';
import '../../cr_elements/cr_dialog/cr_dialog.js';
import '../../cr_elements/icons.html.js';
import '../../cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrDialogElement} from '../../cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from '../../cr_elements/i18n_mixin.js';

import {getTemplate} from './managed_dialog.html.js';

export interface ManagedDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const ManagedDialogElementBase = I18nMixin(PolymerElement);

export class ManagedDialogElement extends ManagedDialogElementBase {
  static get is() {
    return 'managed-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Managed dialog title text. */
      title: String,

      /** Managed dialog body text. */
      body: String,
    };
  }

  override title: string;
  body: string;

  private onOkClick_() {
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'managed-dialog': ManagedDialogElement;
  }
}

customElements.define(ManagedDialogElement.is, ManagedDialogElement);
