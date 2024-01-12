// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {str} from '../../../common/js/translations.js';

import {FileManagerDialogBase} from './file_manager_dialog_base.js';


/**
 * InstallLinuxPackageDialog is used as the handler for .deb files.
 */
export class InstallLinuxPackageDialog extends FileManagerDialogBase {
  private readonly detailsFrame_: HTMLElement;
  private readonly detailsLabel_: HTMLElement;
  private readonly installButton_: HTMLButtonElement;
  private entry_: Entry|null = null;

  constructor(parentNode: HTMLElement) {
    super(parentNode);

    // Creates dialog in DOM tree.
    this.frame.id = 'install-linux-package-dialog';

    this.detailsFrame_ = this.document_.createElement('div');
    this.detailsFrame_.className = 'install-linux-package-details-frame';
    this.frame.insertBefore(this.detailsFrame_, this.buttons);

    this.detailsLabel_ = this.document_.createElement('div');
    this.detailsLabel_.classList.add(
        'install-linux-package-details-label', 'button2');
    this.detailsLabel_.textContent = str('INSTALL_LINUX_PACKAGE_DETAILS_LABEL');

    // The OK button normally dismisses the dialog, so add a button we can
    // customize.
    // Need to copy the whole sub tree because we need child elements.
    this.installButton_ =
        this.okButton.cloneNode(true /* deep */) as HTMLButtonElement;
    // We have child elements inside the button, setting
    // textContent of the button will remove all children.
    this.installButton_.childNodes[0]!.textContent =
        str('INSTALL_LINUX_PACKAGE_INSTALL_BUTTON');

    this.installButton_.addEventListener(
        'click', this.onInstallClick_.bind(this));
    this.buttons.insertBefore(this.installButton_, this.okButton);
    this.initialFocusElement_ = this.installButton_;
  }

  /**
   * Shows the dialog.
   */
  showInstallLinuxPackageDialog(entry: Entry) {
    // We re-use the same object, so reset any visual state that may be
    // changed.
    this.installButton_.hidden = false;
    this.installButton_.disabled = true;
    this.okButton.hidden = true;
    this.cancelButton.hidden = false;

    this.entry_ = entry;

    const title = str('INSTALL_LINUX_PACKAGE_TITLE');
    const message = str('INSTALL_LINUX_PACKAGE_DESCRIPTION');
    const show = super.showOkCancelDialog(title, message);

    if (!show) {
      console.warn('InstallLinuxPackageDialog can\'t be shown.');
      return;
    }

    chrome.fileManagerPrivate.getLinuxPackageInfo(
        this.entry_, this.onGetLinuxPackageInfo_.bind(this));
    this.resetDetailsFrame_(str('INSTALL_LINUX_PACKAGE_DETAILS_LOADING'));
  }

  /**
   * Resets the state of the details frame to just contain the 'Details'
   * label, then appends |message| if non-empty.
   *
   * @param message The (optional) message to display.
   */
  private resetDetailsFrame_(message: string|null) {
    this.detailsFrame_.innerHTML = window.trustedTypes!.emptyHTML;
    this.detailsFrame_.appendChild(this.detailsLabel_);
    if (message) {
      const text = this.document_.createElement('div');
      text.textContent = message;
      text.classList.add('install-linux-package-detail-value', 'body2-primary');
      this.detailsFrame_.appendChild(text);
    }
  }

  /**
   * Updates the dialog with the package info. `linuxPackageInfo` holds the
   * retrieved package info.
   */
  private onGetLinuxPackageInfo_(
      linuxPackageInfo: chrome.fileManagerPrivate.LinuxPackageInfo) {
    if (chrome.runtime.lastError) {
      this.resetDetailsFrame_(
          str('INSTALL_LINUX_PACKAGE_DETAILS_NOT_AVAILABLE'));
      console.warn(
          'Failed to retrieve app info: ' + chrome.runtime.lastError.message);
      return;
    }

    this.resetDetailsFrame_(null);

    const details = [
      [
        str('INSTALL_LINUX_PACKAGE_DETAILS_APPLICATION_LABEL'),
        linuxPackageInfo.name,
      ],
      [
        str('INSTALL_LINUX_PACKAGE_DETAILS_VERSION_LABEL'),
        linuxPackageInfo.version,
      ],
    ];

    // Summary and description are almost always set, but handle the case
    // where they're missing gracefully.
    let description = linuxPackageInfo.summary;
    if (linuxPackageInfo.description) {
      if (description) {
        description += '\n\n';
      }
      description += linuxPackageInfo.description;
    }
    if (description) {
      details.push([
        str('INSTALL_LINUX_PACKAGE_DETAILS_DESCRIPTION_LABEL'),
        description,
      ]);
    }

    this.renderDetails_(details);

    // Allow install now.
    this.installButton_.disabled = false;
  }

  /**
   * @param details Array with pairs:
   *    ['label', 'value'].
   */
  private renderDetails_(details: string[][]) {
    for (const detail of details) {
      const label = this.document_.createElement('div');
      label.textContent = detail[0] + ': ';
      label.className = 'install-linux-package-detail-label';
      const text = this.document_.createElement('div');
      text.textContent = detail[1] || null;
      text.className = 'install-linux-package-detail-value';
      this.detailsFrame_.appendChild(label);
      this.detailsFrame_.appendChild(text);
      this.detailsFrame_.appendChild(this.document_.createElement('br'));
    }
  }

  /**
   * Starts installing the Linux package.
   */
  private onInstallClick_() {
    assert(this.entry_);
    // Add the event listener first to avoid potential races.
    chrome.fileManagerPrivate.installLinuxPackage(
        this.entry_, this.onInstallLinuxPackage_.bind(this));

    this.installButton_.hidden = true;
    this.cancelButton.hidden = true;

    this.okButton.hidden = false;
    this.okButton.focus();
  }

  /**
   * The callback for installLinuxPackage(). Progress updates and completion
   * for successfully started installations will be displayed in a
   * notification, rather than the file manager.
   */
  private onInstallLinuxPackage_(
      status: chrome.fileManagerPrivate.InstallLinuxPackageStatus) {
    if (status ===
        chrome.fileManagerPrivate.InstallLinuxPackageStatus.STARTED) {
      this.text.textContent = str('INSTALL_LINUX_PACKAGE_INSTALLATION_STARTED');
      return;
    }

    // Currently we always display a generic error message. Eventually we'll
    // want a different message for the 'install_already_active' case, and to
    // surface the provided failure reason if one is provided.
    this.title.textContent = str('INSTALL_LINUX_PACKAGE_ERROR_TITLE');
    this.text.textContent = str('INSTALL_LINUX_PACKAGE_ERROR_DESCRIPTION');
    console.warn('Failed to begin package installation.');
  }
}

