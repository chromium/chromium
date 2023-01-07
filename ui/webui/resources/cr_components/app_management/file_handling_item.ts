// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_management_shared_style.css.js';
import './toggle_row.js';

import {assert} from '//resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrDialogElement} from '../../cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from '../../cr_elements/i18n_mixin.js';

import {App} from './app_management.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';
import {AppManagementUserAction} from './constants.js';
import {getTemplate} from './file_handling_item.html.js';
import {AppManagementToggleRowElement} from './toggle_row.js';
import {recordAppManagementUserAction} from './util.js';

const AppManagementFileHandlingItemBase = I18nMixin(PolymerElement);

export class AppManagementFileHandlingItemElement extends
    AppManagementFileHandlingItemBase {
  static get is() {
    return 'app-management-file-handling-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app: Object,

      /**
       * @type {boolean}
       */
      showOverflowDialog: {
        type: Boolean,
        value: false,
      },

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
  showOverflowDialog: boolean;

  override ready() {
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

  private userVisibleTypes_(app: App): string {
    if (app && app.fileHandlingState) {
      return app.fileHandlingState.userVisibleTypes;
    }
    return '';
  }

  private userVisibleTypesLabel_(app: App): string {
    if (app && app.fileHandlingState) {
      return app.fileHandlingState.userVisibleTypesLabel;
    }
    return '';
  }

  private getLearnMoreLinkUrl_(app: App): string {
    if (app && app.fileHandlingState && app.fileHandlingState.learnMoreUrl) {
      return app.fileHandlingState.learnMoreUrl.url;
    }
    return '';
  }

  private onLearnMoreLinkClicked_(e: CustomEvent): void {
    if (!this.getLearnMoreLinkUrl_(this.app)) {
      // Currently, this branch should only be used on Windows.
      e.detail.event.preventDefault();
      e.stopPropagation();
      BrowserProxy.getInstance().handler.showDefaultAppAssociationsUi();
    }
  }

  private launchDialog_(e: CustomEvent): void {
    // A place holder href with the value "#" is used to have a compliant link.
    // This prevents the browser from navigating the window to "#"
    e.detail.event.preventDefault();
    e.stopPropagation();
    this.showOverflowDialog = true;

    recordAppManagementUserAction(
        this.app.type, AppManagementUserAction.FILE_HANDLING_OVERFLOW_SHOWN);
  }

  private onCloseButtonClicked_() {
    this.shadowRoot!.querySelector<CrDialogElement>('#dialog')!.close();
  }

  private onDialogClose_(): void {
    this.showOverflowDialog = false;
    const toFocus = this.shadowRoot!.querySelector<HTMLElement>('#type-list');
    assert(toFocus);
    focusWithoutInk(toFocus);
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
        AppManagementUserAction.FILE_HANDLING_TURNED_ON :
        AppManagementUserAction.FILE_HANDLING_TURNED_OFF;
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
