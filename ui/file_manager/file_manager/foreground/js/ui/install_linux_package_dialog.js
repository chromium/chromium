// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * InstallLinuxPackageDialog is used as the handler for .deb files.
 */
cr.define('cr.filebrowser', function() {
  /**
   * Creates dialog in DOM tree.
   *
   * @param {HTMLElement} parentNode Node to be parent for this dialog.
   * @constructor
   * @extends {FileManagerDialogBase}
   */
  function InstallLinuxPackageDialog(parentNode) {
    FileManagerDialogBase.call(this, parentNode);

    this.frame_.id = 'install-linux-package-dialog';

    this.details_frame_ = this.document_.createElement('div');
    this.details_frame_.className = 'install-linux-package-details-frame';
    this.frame_.insertBefore(this.details_frame_, this.buttons);

    this.details_label_ = this.document_.createElement('div');
    this.details_label_.className = 'install-linux-package-details-label';
    this.details_label_.textContent =
        str('INSTALL_LINUX_PACKAGE_DETAILS_LABEL');

    // The OK button normally dismisses the dialog, so add a button we can
    // customize.
    this.installButton_ = this.okButton_.cloneNode();
    this.installButton_.textContent =
        str('INSTALL_LINUX_PACKAGE_INSTALL_BUTTON');
    this.installButton_.addEventListener(
        'click', this.onInstallClick_.bind(this));
    this.buttons.insertBefore(this.installButton_, this.okButton_);
    this.initialFocusElement_ = this.installButton_;
  }

  InstallLinuxPackageDialog.prototype = {
    __proto__: FileManagerDialogBase.prototype,
  };

  /**
   * Shows the dialog.
   *
   * @param {!Entry} entry
   */
  InstallLinuxPackageDialog.prototype.showInstallLinuxPackageDialog = function(
      entry) {
    // We re-use the same object, so reset any visual state that may be changed.
    this.installButton_.hidden = false;
    this.okButton_.hidden = true;
    this.cancelButton_.hidden = false;

    this.entry_ = entry;

    const title = str('INSTALL_LINUX_PACKAGE_TITLE');
    const message = str('INSTALL_LINUX_PACKAGE_DESCRIPTION');
    const show = FileManagerDialogBase.prototype.showOkCancelDialog.call(
        this, title, message, null, null);

    if (!show) {
      console.error('InstallLinuxPackageDialog can\'t be shown.');
      return;
    }

    chrome.fileManagerPrivate.getLinuxPackageInfo(
        this.entry_, this.onGetLinuxPackageInfo_.bind(this));
    this.resetDetailsFrame_(str('INSTALL_LINUX_PACKAGE_DETAILS_LOADING'));
  };

  /**
   * Resets the state of the details frame to just contain the 'Details' label,
   * then appends |message| if non-empty.
   *
   * @param {string|null} message The (optional) message to display.
   */
  InstallLinuxPackageDialog.prototype.resetDetailsFrame_ = function(message) {
    this.details_frame_.innerHTML = '';
    this.details_frame_.appendChild(this.details_label_);
    if (message) {
      const text = this.document_.createElement('div');
      text.textContent = message;
      text.className = 'install-linux-package-detail-value';
      this.details_frame_.appendChild(text);
    }
  };

  /**
   * Updates the dialog with the package info.
   *
   * @param {(!chrome.fileManagerPrivate.LinuxPackageInfo|undefined)}
   *     linux_package_info The retrieved package info.
   */
  InstallLinuxPackageDialog.prototype.onGetLinuxPackageInfo_ = function(
      linux_package_info) {
    if (chrome.runtime.lastError) {
      this.resetDetailsFrame_(
          str('INSTALL_LINUX_PACKAGE_DETAILS_NOT_AVAILABLE'));
      console.error(
          'Failed to retrieve app info: ' + chrome.runtime.lastError.message);
      return;
    }

    this.resetDetailsFrame_(null);

    const details = [
      [
        str('INSTALL_LINUX_PACKAGE_DETAILS_APPLICATION_LABEL'),
        linux_package_info.name
      ],
      [
        str('INSTALL_LINUX_PACKAGE_DETAILS_VERSION_LABEL'),
        linux_package_info.version
      ],
    ];

    // Summary and description are almost always set, but handle the case
    // where they're missing gracefully.
    let description = linux_package_info.summary;
    if (linux_package_info.description) {
      if (description)
        description += '\n\n';
      description += linux_package_info.description;
    }
    if (description) {
      details.push([
        str('INSTALL_LINUX_PACKAGE_DETAILS_DESCRIPTION_LABEL'), description
      ]);
    }

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
  };

  /**
   * Starts installing the Linux package.
   */
  InstallLinuxPackageDialog.prototype.onInstallClick_ = function() {
    // Add the event listener first to avoid potential races.
    chrome.fileManagerPrivate.installLinuxPackage(
        this.entry_, this.onInstallLinuxPackage_.bind(this));

    this.installButton_.hidden = true;
    this.cancelButton_.hidden = true;

    this.okButton_.hidden = false;
    this.okButton_.focus();
  };

  /**
   * The callback for installLinuxPackage(). Progress updates and completion
   * for succesfully started installations will be displayed in a notification,
   * rather than the file manager.
   * @param {!chrome.fileManagerPrivate.InstallLinuxPackageResponse} response
   *     Whether the install successfully started or not.
   * @param {string} failure_reason A textual reason for the 'failed' case.
   */
  InstallLinuxPackageDialog.prototype.onInstallLinuxPackage_ = function(
      response, failure_reason) {
    if (response == 'started') {
      this.text_.textContent =
          str('INSTALL_LINUX_PACKAGE_INSTALLATION_STARTED');
      return;
    }

    // Currently we always display a generic error message. Eventually we'll
    // want a different message for the 'install_already_active' case, and to
    // surface the provided failure reason if one is provided.
    this.title_.textContent = str('INSTALL_LINUX_PACKAGE_ERROR_TITLE');
    this.text_.textContent = str('INSTALL_LINUX_PACKAGE_ERROR_DESCRIPTION');
    console.error('Failed to begin package installation: ' + failure_reason);
  };

  return {InstallLinuxPackageDialog: InstallLinuxPackageDialog};
});
