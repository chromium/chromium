// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_style.js';
import './toggle_row.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {App} from './app_management.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';
import {AppManagementUserAction, WindowMode} from './constants.js';
import {AppManagementToggleRowElement} from './toggle_row.js';
import {recordAppManagementUserAction} from './util.js';
import {getTemplate} from './window_mode_item.html.js';

export class AppManagementWindowModeElement extends PolymerElement {
  static get is() {
    return 'app-management-window-mode-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      windowModeLabel: String,

      app: Object,

      hidden:
          {type: Boolean, computed: 'isHidden_(app)', reflectToAttribute: true},
    };
  }

  windowModeLabel: String;
  app: App;

  override ready() {
    super.ready();
    this.addEventListener('click', this.onClick_);
    this.addEventListener('change', this.toggleWindowMode_);
  }

  private getValue_(app: App): boolean {
    if (app === undefined) {
      return false;
    }
    assert(app);
    return this.getWindowModeBoolean(app.windowMode);
  }

  private onClick_() {
    this.shadowRoot!
        .querySelector<AppManagementToggleRowElement>('#toggle-row')!.click();
  }

  toggleWindowMode_() {
    assert(this.app);
    const currentWindowMode = this.app.windowMode;
    if (currentWindowMode === WindowMode.kUnknown) {
      assertNotReached();
    }
    const newWindowMode = (currentWindowMode === WindowMode.kBrowser) ?
        WindowMode.kWindow :
        WindowMode.kBrowser;
    BrowserProxy.getInstance().handler.setWindowMode(
        this.app.id,
        newWindowMode,
    );
    const booleanWindowMode = this.getWindowModeBoolean(newWindowMode);
    const windowModeChangeAction = booleanWindowMode ?
        AppManagementUserAction.WINDOW_MODE_CHANGED_TO_WINDOW :
        AppManagementUserAction.WINDOW_MODE_CHANGED_TO_BROWSER;
    recordAppManagementUserAction(this.app.type, windowModeChangeAction);
  }

  private convertWindowModeToBool(windowMode: WindowMode): boolean {
    switch (windowMode) {
      case WindowMode.kBrowser:
        return false;
      case WindowMode.kWindow:
        return true;
      default:
        assertNotReached();
        return false;
    }
  }

  private getWindowModeBoolean(windowMode: WindowMode): boolean {
    assert(windowMode !== WindowMode.kUnknown, 'Window Mode Not Set');
    return this.convertWindowModeToBool(windowMode);
  }

  private isHidden_(app: App): Boolean {
    return app.hideWindowMode;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-window-mode-item': AppManagementWindowModeElement;
  }
}

customElements.define(
    AppManagementWindowModeElement.is, AppManagementWindowModeElement);
