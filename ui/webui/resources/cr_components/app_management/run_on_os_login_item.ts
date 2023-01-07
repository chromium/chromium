// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_management_shared_style.css.js';
import './toggle_row.js';

import {assert, assertNotReached} from '//resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {App} from './app_management.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';
import {AppManagementUserAction, RunOnOsLoginMode} from './constants.js';
import {getTemplate} from './run_on_os_login_item.html.js';
import {AppManagementToggleRowElement} from './toggle_row.js';
import {recordAppManagementUserAction} from './util.js';

function convertModeToBoolean(runOnOsLoginMode: RunOnOsLoginMode): boolean {
  switch (runOnOsLoginMode) {
    case RunOnOsLoginMode.kNotRun:
      return false;
    case RunOnOsLoginMode.kWindowed:
      return true;
    default:
      assertNotReached();
  }
}

function getRunOnOsLoginModeBoolean(runOnOsLoginMode: RunOnOsLoginMode):
    boolean {
  assert(
      runOnOsLoginMode !== RunOnOsLoginMode.kUnknown,
      'Run on OS Login Mode is not set');
  return convertModeToBoolean(runOnOsLoginMode);
}

export class AppManagementRunOnOsLoginItemElement extends PolymerElement {
  static get is() {
    return 'app-management-run-on-os-login-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      loginModeLabel: String,

      app: Object,
    };
  }

  loginModeLabel: string;
  app: App;

  override ready() {
    super.ready();
    this.addEventListener('click', this.onClick_);
    this.addEventListener('change', this.toggleOsLoginMode_);
  }

  private isManaged_(): boolean {
    const loginData = this.app.runOnOsLogin;
    if (loginData) {
      return loginData.isManaged;
    }
    return false;
  }

  private getValue_(): boolean {
    const loginMode = this.getRunOnOsLoginMode();
    assert(loginMode);

    if (loginMode) {
      return getRunOnOsLoginModeBoolean(loginMode);
    }
    return false;
  }

  private onClick_() {
    this.shadowRoot!
        .querySelector<AppManagementToggleRowElement>('#toggle-row')!.click();
  }

  private toggleOsLoginMode_() {
    assert(this.app);
    const currentRunOnOsLoginData = this.app.runOnOsLogin;
    if (currentRunOnOsLoginData) {
      const currentRunOnOsLoginMode = currentRunOnOsLoginData.loginMode;
      if (currentRunOnOsLoginMode === RunOnOsLoginMode.kUnknown) {
        assertNotReached();
      }
      const newRunOnOsLoginMode =
          (currentRunOnOsLoginMode === RunOnOsLoginMode.kNotRun) ?
          RunOnOsLoginMode.kWindowed :
          RunOnOsLoginMode.kNotRun;
      BrowserProxy.getInstance().handler.setRunOnOsLoginMode(
          this.app.id,
          newRunOnOsLoginMode,
      );
      const booleanRunOnOsLoginMode =
          getRunOnOsLoginModeBoolean(newRunOnOsLoginMode);
      const runOnOsLoginModeChangeAction = booleanRunOnOsLoginMode ?
          AppManagementUserAction.RUN_ON_OS_LOGIN_MODE_TURNED_ON :
          AppManagementUserAction.RUN_ON_OS_LOGIN_MODE_TURNED_OFF;
      recordAppManagementUserAction(this.app.type, runOnOsLoginModeChangeAction);
    }
  }

  private getRunOnOsLoginMode(): RunOnOsLoginMode|null {
    if (this.app.runOnOsLogin) {
      return this.app.runOnOsLogin.loginMode;
    }
    return null;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-run-on-os-login-item': AppManagementRunOnOsLoginItemElement;
  }
}

customElements.define(
    AppManagementRunOnOsLoginItemElement.is,
    AppManagementRunOnOsLoginItemElement);
