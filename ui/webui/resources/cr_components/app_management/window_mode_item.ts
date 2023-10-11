// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_management_shared_style.css.js';
import './toggle_row.js';

import {assert, assertNotReached} from '//resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {App} from './app_management.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';
import {AppManagementUserAction, WindowMode} from './constants.js';
import {AppManagementToggleRowElement} from './toggle_row.js';
import {recordAppManagementUserAction} from './util.js';
import {getTemplate} from './window_mode_item.html.js';

function convertWindowModeToBool(windowMode: WindowMode): boolean {
  switch (windowMode) {
    case WindowMode.kBrowser:
      return false;
    case WindowMode.kWindow:
      return true;
    default:
      assertNotReached();
  }
}

function getWindowModeBoolean(windowMode: WindowMode): boolean {
  assert(windowMode !== WindowMode.kUnknown, 'Window Mode Not Set');
  return convertWindowModeToBool(windowMode);
}

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

      hidden: {
        type: Boolean,
        computed: 'isHidden_(app)',
        reflectToAttribute: true,
      },
    };
  }

  windowModeLabel: string;
  app: App;

  override ready() {
    super.ready();
    this.addEventListener('click', this.onClick_);
    this.addEventListener('change', this.toggleWindowMode_);
  }

  private getValue_(): boolean {
    return getWindowModeBoolean(this.app.windowMode);
  }

  private onClick_() {
    this.shadowRoot!
        .querySelector<AppManagementToggleRowElement>('#toggle-row')!.click();
  }

  private toggleWindowMode_() {
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
    const booleanWindowMode = getWindowModeBoolean(newWindowMode);
    const windowModeChangeAction = booleanWindowMode ?
        AppManagementUserAction.WINDOW_MODE_CHANGED_TO_WINDOW :
        AppManagementUserAction.WINDOW_MODE_CHANGED_TO_BROWSER;
    recordAppManagementUserAction(this.app.type, windowModeChangeAction);
  }

  private isHidden_(): boolean {
    return this.app.hideWindowMode;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-window-mode-item': AppManagementWindowModeElement;
  }
}

customElements.define(
    AppManagementWindowModeElement.is, AppManagementWindowModeElement);
