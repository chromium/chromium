// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Manage the installation of apps.
 */
class AppInstaller {
  /**
   * @param {string} itemId Item id to be installed.
   * @param {!CWSWidgetContainerPlatformDelegate} delegate Delegate for
   *     accessing Chrome platform APIs.
   */
  constructor(itemId, delegate) {
    /** @private {!CWSWidgetContainerPlatformDelegate} */
    this.delegate_ = delegate;
    this.itemId_ = itemId;
    this.callback_ = null;
  }

  /**
   * Start an installation.
   * @param {function(AppInstaller.Result, string)} callback Called when the
   *     installation is finished.
   */
  install(callback) {
    this.callback_ = callback;
    this.delegate_.installWebstoreItem(
        this.itemId_, this.onInstallCompleted_.bind(this));
  }

  /**
   * Prevents {@code this.callback_} from being called.
   */
  cancel() {
    // TODO(tbarzic): Would it make sense to uninstall the app on success if the
    // app instaler is cancelled instead of just invalidating the callback?
    this.callback_ = null;
  }

  /**
   * Called when the installation is completed.
   *
   * @param {?string} error Null if the installation is success,
   *     otherwise error message.
   * @private
   */
  onInstallCompleted_(error) {
    if (!this.callback_) {
      return;
    }

    let installerResult = AppInstaller.Result.SUCCESS;
    if (error !== null) {
      installerResult = error == AppInstaller.USER_CANCELLED_ERROR_STR_ ?
          AppInstaller.Result.CANCELLED :
          AppInstaller.Result.ERROR;
    }
    this.callback_(installerResult, error || '');
    this.callback_ = null;
  }
}

/**
 * Type of result.
 *
 * @enum {string}
 * @const
 */
AppInstaller.Result = {
  SUCCESS: 'AppInstaller.success',
  CANCELLED: 'AppInstaller.cancelled',
  ERROR: 'AppInstaller.error'
};
Object.freeze(AppInstaller.Result);

/**
 * Error message for user cancellation. This must be match with the constant
 * 'kUserCancelledError' in C/B/extensions/webstore_standalone_installer.cc.
 * @type {string}
 * @const
 * @private
 */
AppInstaller.USER_CANCELLED_ERROR_STR_ = 'User cancelled install';
