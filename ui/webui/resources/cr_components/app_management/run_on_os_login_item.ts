// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_style.js';
import './toggle_row.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {App} from './app_management.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';
import {AppManagementUserAction, RunOnOsLoginMode} from './constants.js';
import {AppManagementToggleRowElement} from './toggle_row.js';
import {recordAppManagementUserAction} from './util.js';

export class AppManagementRunOnOsLoginItemElement extends PolymerElement {
  static get is() {
    return 'app-management-run-on-os-login-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      loginModeLabel: String,

      app: Object,

      available_: {
        type: Boolean,
        computed: 'isAvailable_(app)',
        reflectToAttribute: true,
      },
    };
  }

  loginModeLabel: String;
  app: App;

  ready() {
    super.ready();
    this.addEventListener('click', this.onClick_);
    this.addEventListener('change', this.toggleOsLoginMode_);
  }

  private isAvailable_(app: App): boolean {
    if (app === undefined) {
      return false;
    }
    assert(app);
    return app.runOnOsLogin !== undefined;
  }

  private isManaged_(app: App): boolean {
    if (app === undefined || !this.isAvailable_(app)) {
      return false;
    }
    assert(app);

    const loginData = app.runOnOsLogin;
    if (loginData) {
      return loginData.isManaged;
    }
    return false;
  }


  private getValue_(app: App): boolean {
    if (app === undefined) {
      return false;
    }
    assert(app);

    const loginMode = this.getRunOnOsLoginMode(app);
    assert(loginMode);

    if (loginMode) {
      return this.getRunOnOsLoginModeBoolean(loginMode);
    }
    return false;
  }

  private onClick_() {
    this.shadowRoot!
        .querySelector<AppManagementToggleRowElement>('#toggle-row')!.click();
  }

  toggleOsLoginMode_() {
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
      const booleanWindowMode =
          this.getRunOnOsLoginModeBoolean(newRunOnOsLoginMode);
      const windowModeChangeAction = booleanWindowMode ?
          AppManagementUserAction.RunOnOsLoginModeTurnedOn :
          AppManagementUserAction.RunOnOsLoginModeTurnedOff;
      recordAppManagementUserAction(this.app.type, windowModeChangeAction);
    }
  }

  private getRunOnOsLoginMode(app: App): RunOnOsLoginMode|null {
    if (app.runOnOsLogin) {
      return app.runOnOsLogin.loginMode;
    }
    return null;
  }

  private convertModeToBoolean(runOnOsLoginMode: RunOnOsLoginMode): boolean {
    switch (runOnOsLoginMode) {
      case RunOnOsLoginMode.kNotRun:
        return false;
      case RunOnOsLoginMode.kWindowed:
        return true;
      default:
        assertNotReached();
        return false;
    }
  }

  private getRunOnOsLoginModeBoolean(runOnOsLoginMode: RunOnOsLoginMode):
      boolean {
    assert(
        runOnOsLoginMode !== RunOnOsLoginMode.kUnknown,
        'Run on OS Login Mode is not set');
    return this.convertModeToBoolean(runOnOsLoginMode);
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
