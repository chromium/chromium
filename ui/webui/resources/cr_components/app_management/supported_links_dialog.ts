// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_management_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {App} from './app_management.mojom-webui.js';
import {getTemplate} from './supported_links_dialog.html.js';

const AppManagementSupportedLinksDialogElementBase = I18nMixin(PolymerElement);

export class AppManagementSupportedLinksDialogElement extends
    AppManagementSupportedLinksDialogElementBase {
  static get is() {
    return 'app-management-supported-links-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app: Object,
    };
  }

  app: App;
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-supported-links-dialog':
        AppManagementSupportedLinksDialogElement;
  }
}

customElements.define(
    AppManagementSupportedLinksDialogElement.is,
    AppManagementSupportedLinksDialogElement);
