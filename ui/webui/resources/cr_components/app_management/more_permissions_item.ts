// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_management_shared_style.css.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {App} from './app_management.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';
import {AppManagementUserAction} from './constants.js';
import {getTemplate} from './more_permissions_item.html.js';
import {recordAppManagementUserAction} from './util.js';

export class AppManagementMorePermissionsItemElement extends PolymerElement {
  static get is() {
    return 'app-management-more-permissions-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app: Object,
      morePermissionsLabel: String,
    };
  }

  app: App;
  morePermissionsLabel: string;

  override ready() {
    super.ready();
    this.addEventListener('click', this.onClick_);
  }

  private onClick_() {
    BrowserProxy.getInstance().handler.openNativeSettings(this.app.id);
    recordAppManagementUserAction(
        this.app.type, AppManagementUserAction.NATIVE_SETTINGS_OPENED);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-more-permissions-item':
        AppManagementMorePermissionsItemElement;
  }
}

customElements.define(
    AppManagementMorePermissionsItemElement.is,
    AppManagementMorePermissionsItemElement);
