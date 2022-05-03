// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_style.js';
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/policy/cr_tooltip_icon.m.js';

import {assertNotReached} from '//resources/js/assert.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {App} from './app_management.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';
import {AppManagementUserAction, InstallReason} from './constants.js';
import {getTemplate} from './uninstall_button.html.js';
import {recordAppManagementUserAction} from './util.js';

export class AppManamentUninstallButtonElement extends PolymerElement {
  static get is() {
    return 'app-management-uninstall-button';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app: Object,
      uninstallLabel: String,
      policyLabel: String,
    };
  }

  app: App;
  uninstallLabel: string;
  policyLabel: string;

  /**
   * Returns true if the button should be disabled due to app install type.
   */
  getDisableState_(app: App): boolean {
    if (!app) {
      return true;
    }

    switch (app.installReason) {
      case InstallReason.kSystem:
      case InstallReason.kPolicy:
        return true;
      case InstallReason.kOem:
      case InstallReason.kDefault:
      case InstallReason.kSync:
      case InstallReason.kUser:
      case InstallReason.kUnknown:
        return false;
      default:
        assertNotReached();
        return false;
    }
  }

  /**
   * Returns true if the app was installed by a policy.
   */
  private showPolicyIndicator_(app: App): boolean {
    if (!app) {
      return false;
    }
    return app.installReason === InstallReason.kPolicy;
  }

  /**
   * Returns true if the uninstall button should be shown.
   */
  private showUninstallButton_(app: App): boolean {
    if (!app) {
      return false;
    }
    return app.installReason !== InstallReason.kSystem;
  }

  private onClick_() {
    BrowserProxy.getInstance().handler.uninstall(this.app.id);
    recordAppManagementUserAction(
        this.app.type, AppManagementUserAction.UNINSTALL_DIALOG_LAUNCHED);
  }
}

customElements.define(
    AppManamentUninstallButtonElement.is, AppManamentUninstallButtonElement);
