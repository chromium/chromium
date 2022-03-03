// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_style.js';
import './toggle_row.js';

import {assert} from '//resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {App} from './app_management.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';
import {AppManagementUserAction} from './constants.js';
import {AppManagementToggleRowElement} from './toggle_row.js';
import {recordAppManagementUserAction} from './util.js';

export class AppManagementFileHandlingItemElement extends PolymerElement {
  static get is() {
    return 'app-management-file-handling-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      fileHandlingHeader: String,
      fileHandlingSetDefaults: String,

      app: Object,

      /**
       * @type {boolean}
       */
      hidden: {
        type: Boolean,
        computed: 'isHidden_(app)',
        reflectToAttribute: true,
      },
    };
  }

  app: App;
  fileHandlingHeader: String;
  fileHandlingSetDefaults: String;

  ready() {
    super.ready();
    this.addEventListener('change', this.onChanged_);
  }

  private isHidden_(app: App): boolean {
    if (app && app.fileHandlingState) {
      return !app.fileHandlingState.userVisibleTypes;
    }
    return false;
  }

  private isManaged_(app: App): boolean {
    if (app && app.fileHandlingState) {
      return app.fileHandlingState.isManaged;
    }
    return false;
  }

  private userVisibleTypesLabel_(app: App): string {
    if (app && app.fileHandlingState) {
      return app.fileHandlingState.userVisibleTypesLabel;
    }
    return '';
  }

  private getLearnMoreLinkUrl_(app: App): string {
    if (app && app.fileHandlingState) {
      return app.fileHandlingState.learnMoreUrl.url;
    }
    return '';
  }

  private getValue_(app: App): boolean {
    if (app && app.fileHandlingState) {
      return app.fileHandlingState.enabled;
    }
    return false;
  }

  private onChanged_() {
    assert(this.app);
    const enabled = this.shadowRoot!
                        .querySelector<AppManagementToggleRowElement>(
                            '#toggle-row')!.isChecked();

    BrowserProxy.getInstance().handler.setFileHandlingEnabled(
        this.app.id,
        enabled,
    );
    const fileHandlingChangeAction = enabled ?
        AppManagementUserAction.FileHandlingTurnedOn :
        AppManagementUserAction.FileHandlingTurnedOff;
    recordAppManagementUserAction(this.app.type, fileHandlingChangeAction);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-file-handling-item': AppManagementFileHandlingItemElement;
  }
}

customElements.define(
    AppManagementFileHandlingItemElement.is,
    AppManagementFileHandlingItemElement);
