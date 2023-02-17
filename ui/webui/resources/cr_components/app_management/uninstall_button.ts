// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_management_shared_style.css.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/policy/cr_tooltip_icon.js';

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
   *
   * If the compiler complains about the "lack of ending return statement",
   * you maybe just added a new InstallReason and need to add a new case.
   */
  private getDisableState_(): boolean {
    switch (this.app.installReason) {
      case InstallReason.kSystem:
      case InstallReason.kPolicy:
      case InstallReason.kKiosk:
        return true;
      case InstallReason.kUnknown:
      case InstallReason.kOem:
      case InstallReason.kDefault:
      case InstallReason.kSync:
      case InstallReason.kUser:
      case InstallReason.kSubApp:
      case InstallReason.kCommandLine:
        return false;
    }
  }

  /**
   * Returns true if the app was installed by a policy.
   */
  private showPolicyIndicator_(): boolean {
    return this.app.installReason === InstallReason.kPolicy;
  }

  /**
   * Returns true if the uninstall button should be shown.
   */
  private showUninstallButton_(): boolean {
    return this.app.installReason !== InstallReason.kSystem;
  }

  private onClick_() {
    BrowserProxy.getInstance().handler.uninstall(this.app.id);
    recordAppManagementUserAction(
        this.app.type, AppManagementUserAction.UNINSTALL_DIALOG_LAUNCHED);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-uninstall-button': AppManamentUninstallButtonElement;
  }
}

customElements.define(
    AppManamentUninstallButtonElement.is, AppManamentUninstallButtonElement);
