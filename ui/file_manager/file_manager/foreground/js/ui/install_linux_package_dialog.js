// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {isJellyEnabled} from '../../../common/js/flags.js';
import {str} from '../../../common/js/translations.js';

import {FileManagerDialogBase} from './file_manager_dialog_base.js';


/**
 * InstallLinuxPackageDialog is used as the handler for .deb files.
 */
/**
 * Creates dialog in DOM tree.
 */
// @ts-ignore: error TS2415: Class 'InstallLinuxPackageDialog' incorrectly
// extends base class 'FileManagerDialogBase'.
export class InstallLinuxPackageDialog extends FileManagerDialogBase {
  /**
   * @param {HTMLElement} parentNode Node to be parent for this dialog.
   */
  constructor(parentNode) {
    super(parentNode);

    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.frame.id = 'install-linux-package-dialog';

    this.details_frame_ = this.document_.createElement('div');
    this.details_frame_.className = 'install-linux-package-details-frame';
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.frame.insertBefore(this.details_frame_, this.buttons);

    this.details_label_ = this.document_.createElement('div');
    this.details_label_.classList.add(
        'install-linux-package-details-label', 'button2');
    this.details_label_.textContent =
        str('INSTALL_LINUX_PACKAGE_DETAILS_LABEL');

    // The OK button normally dismisses the dialog, so add a button we can
    // customize.
    if (isJellyEnabled()) {
      // Need to copy the whole sub tree because we need child elements.
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.installButton_ = this.okButton.cloneNode(true /* deep */);
      // When Jelly is on, we have child elements inside the button, setting
      // textContent of the button will remove all children.
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      this.installButton_.childNodes[0].textContent =
          str('INSTALL_LINUX_PACKAGE_INSTALL_BUTTON');
    } else {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.installButton_ = this.okButton.cloneNode(false /* deep */);
      this.installButton_.textContent =
          str('INSTALL_LINUX_PACKAGE_INSTALL_BUTTON');
    }
    this.installButton_.addEventListener(
        'click', this.onInstallClick_.bind(this));
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.buttons.insertBefore(this.installButton_, this.okButton);
    this.initialFocusElement_ = this.installButton_;

    /** @private @type {?Entry} */
    this.entry_ = null;
  }

  /**
   * Shows the dialog.
   *
   * @param {!Entry} entry
   */
  showInstallLinuxPackageDialog(entry) {
    // We re-use the same object, so reset any visual state that may be
    // changed.
    // @ts-ignore: error TS2339: Property 'hidden' does not exist on type
    // 'Node'.
    this.installButton_.hidden = false;
    // @ts-ignore: error TS2339: Property 'disabled' does not exist on type
    // 'Node'.
    this.installButton_.disabled = true;
    // @ts-ignore: error TS2339: Property 'hidden' does not exist on type
    // 'Element'.
    this.okButton.hidden = true;
    // @ts-ignore: error TS2339: Property 'hidden' does not exist on type
    // 'Element'.
    this.cancelButton.hidden = false;

    this.entry_ = entry;

    const title = str('INSTALL_LINUX_PACKAGE_TITLE');
    const message = str('INSTALL_LINUX_PACKAGE_DESCRIPTION');
    const show = super.showOkCancelDialog(title, message, null, null);

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
   * @param {string|null} message The (optional) message to display.
   */
  resetDetailsFrame_(message) {
    // @ts-ignore: error TS2304: Cannot find name 'trustedTypes'.
    this.details_frame_.innerHTML = trustedTypes.emptyHTML;
    this.details_frame_.appendChild(this.details_label_);
    if (message) {
      const text = this.document_.createElement('div');
      text.textContent = message;
      text.classList.add('install-linux-package-detail-value', 'body2-primary');
      this.details_frame_.appendChild(text);
    }
  }

  /**
   * Updates the dialog with the package info.
   *
   * @param {(!chrome.fileManagerPrivate.LinuxPackageInfo|undefined)}
   *     linux_package_info The retrieved package info.
   */
  onGetLinuxPackageInfo_(linux_package_info) {
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
        // @ts-ignore: error TS18048: 'linux_package_info' is possibly
        // 'undefined'.
        linux_package_info.name,
      ],
      [
        str('INSTALL_LINUX_PACKAGE_DETAILS_VERSION_LABEL'),
        // @ts-ignore: error TS18048: 'linux_package_info' is possibly
        // 'undefined'.
        linux_package_info.version,
      ],
    ];

    // Summary and description are almost always set, but handle the case
    // where they're missing gracefully.
    // @ts-ignore: error TS18048: 'linux_package_info' is possibly 'undefined'.
    let description = linux_package_info.summary;
    // @ts-ignore: error TS18048: 'linux_package_info' is possibly 'undefined'.
    if (linux_package_info.description) {
      if (description) {
        description += '\n\n';
      }
      // @ts-ignore: error TS18048: 'linux_package_info' is possibly
      // 'undefined'.
      description += linux_package_info.description;
    }
    if (description) {
      details.push([
        str('INSTALL_LINUX_PACKAGE_DETAILS_DESCRIPTION_LABEL'),
        description,
      ]);
    }

    this.renderDetails_(details);

    // Allow install now.
    // @ts-ignore: error TS2339: Property 'disabled' does not exist on type
    // 'Node'.
    this.installButton_.disabled = false;
  }

  /**
   * @param {!Array<!Array<string>>} details Array with pairs:
   *    ['label', 'value'].
   * @private
   */
  renderDetails_(details) {
    for (const detail of details) {
      const label = this.document_.createElement('div');
      label.textContent = detail[0] + ': ';
      label.className = 'install-linux-package-detail-label';
      const text = this.document_.createElement('div');
      text.textContent = detail[1];
      text.className = 'install-linux-package-detail-value';
      this.details_frame_.appendChild(label);
      this.details_frame_.appendChild(text);
      this.details_frame_.appendChild(this.document_.createElement('br'));
    }
  }

  /**
   * Starts installing the Linux package.
   */
  onInstallClick_() {
    // Add the event listener first to avoid potential races.
    chrome.fileManagerPrivate.installLinuxPackage(
        // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry | null'
        // is not assignable to parameter of type 'FileSystemEntry'.
        assert(this.entry_), this.onInstallLinuxPackage_.bind(this));

    // @ts-ignore: error TS2339: Property 'hidden' does not exist on type
    // 'Node'.
    this.installButton_.hidden = true;
    // @ts-ignore: error TS2339: Property 'hidden' does not exist on type
    // 'Element'.
    this.cancelButton.hidden = true;

    // @ts-ignore: error TS2339: Property 'hidden' does not exist on type
    // 'Element'.
    this.okButton.hidden = false;
    // @ts-ignore: error TS2339: Property 'focus' does not exist on type
    // 'Element'.
    this.okButton.focus();
  }

  /**
   * The callback for installLinuxPackage(). Progress updates and completion
   * for successfully started installations will be displayed in a
   * notification, rather than the file manager.
   * @param {!chrome.fileManagerPrivate.InstallLinuxPackageResponse} response
   *     Whether the install successfully started or not.
   * @param {string} failure_reason A textual reason for the 'failed' case.
   */
  onInstallLinuxPackage_(response, failure_reason) {
    if (response == 'started') {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.text.textContent = str('INSTALL_LINUX_PACKAGE_INSTALLATION_STARTED');
      return;
    }

    // Currently we always display a generic error message. Eventually we'll
    // want a different message for the 'install_already_active' case, and to
    // surface the provided failure reason if one is provided.
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.title.textContent = str('INSTALL_LINUX_PACKAGE_ERROR_TITLE');
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.text.textContent = str('INSTALL_LINUX_PACKAGE_ERROR_DESCRIPTION');
    console.warn('Failed to begin package installation: ' + failure_reason);
  }
}
