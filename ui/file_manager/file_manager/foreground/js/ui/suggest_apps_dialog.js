// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * SuggestAppsDialog contains a list box to select an app to be opened the file
 * with. This dialog should be used as action picker for file operations.
 */


/**
 * Creates dialog in DOM tree.
 *
 */
class SuggestAppsDialog extends FileManagerDialogBase {
  /**
   * @param {!ProvidersModel} providersModel Model for providers.
   * @param {!HTMLElement} parentNode Node to be parent for this dialog.
   * @param {!SuggestAppDialogState} state Static state of suggest app dialog.
   */
  constructor(providersModel, parentNode, state) {
    super(parentNode);

    /**
     * @private {!ProvidersModel}
     * @const
     */
    this.providersModel_ = providersModel;

    this.frame.id = 'suggest-app-dialog';

    /**
     * The root element for the Chrome Web Store widget container.
     * @const {!HTMLElement}
     */
    const widgetRoot = this.document_.createElement('div');
    this.frame.insertBefore(widgetRoot, this.text.nextSibling);

    /**
     * The wrapper around Chrome Web Store widget.
     * @const {!CWSWidgetContainer}
     * @private
     */
    this.widget_ = new CWSWidgetContainer(
        this.document_, widgetRoot, this.createWidgetPlatformDelegate_(),
        state);

    this.initialFocusElement_ = this.widget_.getInitiallyFocusedElement();

    /**
     * The reported widget result.
     * @type {SuggestAppsDialog.Result}
     * @private
     */
    this.result_ = SuggestAppsDialog.Result.FAILED;

    // Hide default dialog buttons.
    this.buttons.hidden = true;

    // Override default dialog styles.
    this.title.classList.add('suggest-apps-dialog-title');
    this.text.classList.add('suggest-apps-dialog-text');

    /** @private {?string} */
    this.installedItemId_ = null;

    /** @private {?function(SuggestAppsDialog.Result, ?string)} */
    this.onDialogClosed_ = null;

    /** @private {string} */
    this.dialogText_ = '';
  }

  /**
   * Dummy function for SuggestAppsDialog.show() not to be called
   * unintentionally.
   */
  show() {
    console.error('SuggestAppsDialog.show() shouldn\'t be called directly.');
  }

  /**
   * Shows suggest-apps dialog by file extension and mime.
   *
   * @param {string} extension Extension of the file with a trailing dot.
   * @param {?string} mime Mime of the file.
   * @param {function(SuggestAppsDialog.Result, ?string)} onDialogClosed Called
   *     when the dialog is closed, with a result code and an optionally an
   *     extension id, if an extension was installed.
   */
  showByExtensionAndMime(extension, mime, onDialogClosed) {
    assert(extension && extension[0] === '.');
    const options = {file_extension: extension.substr(1)};
    if (mime) {
      options.mime_type = mime;
    }
    this.showInternal_(
        options, str('SUGGEST_DIALOG_TITLE'),
        webStoreUtils.createWebStoreLink(extension, mime), onDialogClosed);
  }

  /**
   * Shows suggest-apps dialog for FSP API
   * @param {function(SuggestAppsDialog.Result, ?string)} onDialogClosed Called
   *     when the dialog is closed, with a result code and an optionally an
   *     extension id, if an extension was installed.
   */
  showProviders(onDialogClosed) {
    this.showInternal_(
        {file_system_provider: true}, str('SUGGEST_DIALOG_FOR_PROVIDERS_TITLE'),
        null /* webStoreUrl */, onDialogClosed);
  }

  /**
   * Creates platform delegate for CWSWidgetContainer.
   * @return {!CWSWidgetContainerPlatformDelegate}
   * @private
   */
  createWidgetPlatformDelegate_() {
    return {
      strings: {
        UI_LOCALE: util.getCurrentLocaleOrDefault(),
        LINK_TO_WEBSTORE: str('SUGGEST_DIALOG_LINK_TO_WEBSTORE'),
        INSTALLATION_FAILED_MESSAGE: str('SUGGEST_DIALOG_INSTALLATION_FAILED'),
        LOADING_SPINNER_ALT: str('SUGGEST_DIALOG_LOADING_SPINNER_ALT'),
        INSTALLING_SPINNER_ALT: str('SUGGEST_DIALOG_INSTALLING_SPINNER_ALT')
      },

      metricsImpl: {
        /**
         * @param {string} enumName
         * @param {number} value
         * @param {number} enumSize
         */
        recordEnum: function(enumName, value, enumSize) {
          metrics.recordEnum('SuggestApps.' + enumName, value, enumSize);
        },

        /** @param {string} actionName */
        recordUserAction: function(actionName) {
          metrics.recordUserAction('SuggestApps.' + actionName);
        },

        /** @param {string} intervalName */
        startInterval: function(intervalName) {
          metrics.startInterval('SuggestApps.' + intervalName);
        },

        /** @param {string} intervalName */
        recordInterval: function(intervalName) {
          metrics.recordInterval('SuggestApps.' + intervalName);
        }
      },

      /**
       * @param {string} itemId,
       * @param {function(?string)} callback Callback argument is set to error
       *     message (null on success)
       */
      installWebstoreItem: function(itemId, callback) {
        chrome.webstoreWidgetPrivate.installWebstoreItem(
            itemId, false /* show installation prompt */, () => {
              callback(
                  chrome.runtime.lastError ?
                      chrome.runtime.lastError.message || 'UNKNOWN ERROR' :
                      null);
            });
      },

      /**
       * @param {function(?Array<!string>)} callback Callback
       *     argument is a list of installed item ids (null on error).
       */
      getInstalledItems: callback => {
        // Return only installed provided extensions. Returning other
        // extensions/apps is redundant, as the suggest app for non-providers is
        // executed only when there is no extension/app matching a file task.
        // Hence, none of the suggested extensions/apps can be already
        // installed.
        this.providersModel_.getInstalledProviders()
            .then(providers => {
              callback(providers.map(provider => {
                // Assume that the provider is an extension backed provider. In
                // such case the providerId is the same as extensionId.
                return provider.providerId;
              }));
            })
            .catch(error => {
              console.error(error.stack || error);
              callback(null);
            });
      },

      /**
       * @param {function(?string)} callback Callback argument is the requested
       *     token (null on error).
       */
      requestWebstoreAccessToken: function(callback) {
        chrome.fileManagerPrivate.requestWebStoreAccessToken(token => {
          if (chrome.runtime.lastError) {
            console.error(chrome.runtime.lastError.message);
            callback(null);
            return;
          }
          callback(assert(token));
        });
      }
    };
  }

  /**
   * Internal method to show a dialog. This should be called only from 'Suggest.
   * appDialog.showXxxx()' functions.
   *
   * @param {!Object<*>} options Map of options for the dialog.
   * @param {string} title Title of the dialog.
   * @param {?string} webStoreUrl Url for more results. Null if not supported.
   * @param {function(SuggestAppsDialog.Result, ?string)} onDialogClosed Called
   *     when the dialog is closed, with a result code and an optionally an
   *     extension id, if an extension was installed.
   * @private
   */
  showInternal_(options, title, webStoreUrl, onDialogClosed) {
    this.text.hidden = true;
    this.dialogText_ = '';

    if (!this.widget_.isInInitialState()) {
      onDialogClosed(SuggestAppsDialog.Result.CANCELLED, null);
      return;
    }

    let dialogShown = false;
    let tokenObtained = false;

    this.widget_.ready()
        .then(/** @return {!Promise} */
              () => {
                tokenObtained = true;
                return this.showDialog_(title);
              })
        .then(/** @return {!Promise<CWSWidgetContainer.ResolveReason>} */
              () => {
                dialogShown = true;
                // This is not set before so it doesn't pollute state if the
                // previous dialog hasn't finished hiding.
                this.onDialogClosed_ = onDialogClosed;
                return this.widget_.start(options, webStoreUrl);
              })
        .then(/** @param {CWSWidgetContainer.ResolveReason} reason */
              reason => {
                if (reason !== CWSWidgetContainer.ResolveReason.RESET) {
                  this.hide();
                }
              })
        .catch(error => {
          console.error('Failed to start CWS widget: ' + error);

          if (!dialogShown) {
            // Reset any widget state set in |this.widget_.ready()|. The
            // returned value is ignored because it doesn't influence the
            // value reported by dialog.
            this.widget_.finalizeAndGetResult();

            const result = tokenObtained ?
                // Got access token but the widget dialog was not shown.
                // Consider the widget was cancelled.
                SuggestAppsDialog.Result.CANCELLED :
                // Access token was unavailable.
                // This can happen in the Guest mode. crbug.com/694419
                // Callback shows an alert notifying the file was not opened
                // because of the unsupported type.
                SuggestAppsDialog.Result.FAILED;
            onDialogClosed(result, null);
            return;
          }

          this.result_ = SuggestAppsDialog.Result.FAILED;
          this.hide();
        });
  }

  /**
   * Internal method for showing the dialog in the file manager window.
   * @param {string} title The dialog title.
   * @return {!Promise}
   */
  showDialog_(title) {
    return new Promise((resolve, reject) => {
      const success = this.dialogText_ ?
          super.showTitleAndTextDialog(title, this.dialogText_) :
          super.showTitleOnlyDialog(title);
      if (!success) {
        reject('SuggestAppsDialog cannot be shown.');
        return;
      }
      resolve();
    });
  }

  /**
   * Called when the connection status is changed.
   * @param {VolumeManagerCommon.DriveConnectionType} connectionType Current
   *     connection type.
   */
  onDriveConnectionChanged(connectionType) {
    if (connectionType === VolumeManagerCommon.DriveConnectionType.OFFLINE) {
      this.widget_.onConnectionLost();
    }
  }

  /**
   * @param {Function=} opt_originalOnHide Called when the original dialog is
   *     hidden.
   * @override
   */
  hide(opt_originalOnHide) {
    const widgetResult = this.widget_.finalizeAndGetResult();

    switch (widgetResult.result) {
      case CWSWidgetContainer.Result.INSTALL_SUCCESSFUL:
        this.result_ = SuggestAppsDialog.Result.SUCCESS;
        break;
      case CWSWidgetContainer.Result.WEBSTORE_LINK_OPENED:
      case CWSWidgetContainer.Result.USER_CANCEL:
        this.result_ = SuggestAppsDialog.Result.CANCELLED;
        break;
      default:
        this.result_ = SuggestAppsDialog.Result.FAILED;
    }

    this.installedItemId_ = widgetResult.installedItemId;

    super.hide(this.onHide_.bind(this, opt_originalOnHide));
  }

  /**
   * @param {Function=} opt_originalOnHide Original onHide function passed to
   *     SuggestAppsDialog.hide().
   * @private
   */
  onHide_(opt_originalOnHide) {
    // Calls the callback after the dialog hides.
    if (opt_originalOnHide) {
      opt_originalOnHide();
    }

    this.onDialogClosed_(this.result_, this.installedItemId_);
  }
}

/**
 * @enum {string}
 */
SuggestAppsDialog.Result = {
  // Install is done. The install app should be opened.
  SUCCESS: 'SuggestAppsDialog.Result.SUCCESS',
  // User cancelled the suggest app dialog. No message should be shown.
  CANCELLED: 'SuggestAppsDialog.Result.CANCELLED',
  // Failed to load the widget. Error message should be shown.
  FAILED: 'SuggestAppsDialog.Result.FAILED'
};
Object.freeze(SuggestAppsDialog.Result);
