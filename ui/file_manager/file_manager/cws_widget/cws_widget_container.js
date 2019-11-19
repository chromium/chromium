// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * CWSWidgetContainer contains a Chrome Web Store widget that displays list of
 * apps that satisfy certain constraints (e.g. fileHandler apps that can handle
 * files with specific file extension or MIME type) and enables the user to
 * install apps directly from it.
 * CWSWidgetContainer implements client side of the widget, which handles
 * widget loading and app installation.
 */

/**
 * The width of the widget (in pixels)
 * @type {number}
 * @const
 */
const WEBVIEW_WIDTH = 735;

/**
 * The height of the widget (in pixels).
 * @type {number}
 * @const
 */
const WEBVIEW_HEIGHT = 480;

/**
 * The URL of the widget showing suggested apps.
 * @type {string}
 * @const
 */
const CWS_WIDGET_URL =
    'https://clients5.google.com/webstore/wall/cros-widget-container';

/**
 * The origin of the widget.
 * @type {string}
 * @const
 */
const CWS_WIDGET_ORIGIN = 'https://clients5.google.com';

/**
 * Creates the widget container element in DOM tree.
 */
class CWSWidgetContainer {
  /**
   *
   * @param {!HTMLDocument} document The document to contain this container.
   * @param {!HTMLElement} parentNode Node to be parent for this container.
   * @param {!CWSWidgetContainerPlatformDelegate} delegate Delegate for
   *     accessing Chrome platform APIs.
   * @param {!{
   *   overrideCwsContainerUrlForTest: (string|undefined),
   *   overrideCwsContainerOriginForTest: (string|undefined)
   * }} params Overrides for container params.
   */
  constructor(document, parentNode, delegate, params) {
    /** @private {!CWSWidgetContainerPlatformDelegate} */
    this.delegate_ = delegate;

    /** @private {!CWSWidgetContainer.MetricsRecorder} */
    this.metricsRecorder_ =
        new CWSWidgetContainer.MetricsRecorder(delegate.metricsImpl);

    /**
     * The document that will contain the container.
     * @const {!HTMLDocument}
     * @private
     */
    this.document_ = document;

    /**
     * The element containing the widget webview.
     * @type {!Element}
     * @private
     */
    this.webviewContainer_ = document.createElement('div');
    this.webviewContainer_.classList.add('cws-widget-webview-container');
    this.webviewContainer_.style.width = WEBVIEW_WIDTH + 'px';
    this.webviewContainer_.style.height = WEBVIEW_HEIGHT + 'px';
    parentNode.appendChild(this.webviewContainer_);

    parentNode.classList.add('cws-widget-container-root');

    /**
     * Element showing spinner layout in place of Web Store widget.
     * @type {!Element}
     */
    const spinnerLayer = document.createElement('div');
    spinnerLayer.className = 'cws-widget-spinner-layer';
    parentNode.appendChild(spinnerLayer);

    /** @private {!CWSWidgetContainer.SpinnerLayerController} */
    this.spinnerLayerController_ =
        new CWSWidgetContainer.SpinnerLayerController(spinnerLayer);

    /**
     * The widget container's button strip.
     * @type {!Element}
     */
    const buttons = document.createElement('div');
    buttons.classList.add('cws-widget-buttons');
    parentNode.appendChild(buttons);

    /**
     * Button that opens the Webstore URL.
     * @const {!Element}
     * @private
     */
    this.webstoreButton_ = document.createElement('div');
    this.webstoreButton_.hidden = true;
    this.webstoreButton_.setAttribute('role', 'button');
    this.webstoreButton_.tabIndex = 0;

    /**
     * Icon for the Webstore button.
     * @type {!Element}
     */
    const webstoreButtonIcon = this.document_.createElement('span');
    webstoreButtonIcon.classList.add('cws-widget-webstore-button-icon');
    this.webstoreButton_.appendChild(webstoreButtonIcon);

    /**
     * The label for the Webstore button.
     * @type {!Element}
     */
    const webstoreButtonLabel = this.document_.createElement('span');
    webstoreButtonLabel.classList.add('cws-widget-webstore-button-label');
    webstoreButtonLabel.textContent = this.delegate_.strings.LINK_TO_WEBSTORE;
    this.webstoreButton_.appendChild(webstoreButtonLabel);

    this.webstoreButton_.addEventListener(
        'click', this.onWebstoreLinkActivated_.bind(this));
    this.webstoreButton_.addEventListener(
        'keydown', this.onWebstoreLinkKeyDown_.bind(this));

    buttons.appendChild(this.webstoreButton_);

    /**
     * The webview element containing the Chrome Web Store widget.
     * @type {?WebView}
     * @private
     */
    this.webview_ = null;

    /**
     * The Chrome Web Store widget URL.
     * @const {string}
     * @private
     */
    this.widgetUrl_ = params.overrideCwsContainerUrlForTest || CWS_WIDGET_URL;

    /**
     * The Chrome Web Store widget origin.
     * @const {string}
     * @private
     */
    this.widgetOrigin_ =
        params.overrideCwsContainerOriginForTest || CWS_WIDGET_ORIGIN;

    /**
     * Map of options for the widget.
     * @type {?Object<*>}
     * @private
     */
    this.options_ = null;

    /**
     * The ID of the item being installed. Null if no items are being installed.
     * @type {?string}
     * @private
     */
    this.installingItemId_ = null;

    /**
     * The ID of the the installed item. Null if no item was installed.
     * @type {?string}
     * @private
     */
    this.installedItemId_ = null;

    /**
     * The current widget state.
     * @type {CWSWidgetContainer.State}
     * @private
     */
    this.state_ = CWSWidgetContainer.State.UNINITIALIZED;

    /**
     * The Chrome Web Store access token to be used when communicating with the
     * Chrome Web Store widget.
     * @type {?string}
     * @private
     */
    this.accessToken_ = null;

    /**
     * Called when the Chrome Web Store widget is done. It resolves the promise
     * returned by {@code this.start()}.
     * @type {?function(CWSWidgetContainer.ResolveReason)}
     * @private
     */
    this.resolveStart_ = null;

    /**
     * Promise for retrieving {@code this.accessToken_}.
     * @type {Promise<string>}
     * @private
     */
    this.tokenGetter_ = this.createTokenGetter_();

    /**
     * Dialog to be shown when an installation attempt fails.
     * @type {CWSWidgetContainerErrorDialog}
     * @private
     */
    this.errorDialog_ = new CWSWidgetContainerErrorDialog(parentNode);

    /** @private {?AppInstaller} */
    this.appInstaller_ = null;

    /** @private {?CWSContainerClient} */
    this.webviewClient_ = null;

    /** @private {?string} */
    this.webStoreUrl_ = null;
  }

  /**
   * @return {!Element} The element that should be focused initially.
   */
  getInitiallyFocusedElement() {
    return this.webviewContainer_;
  }

  /**
   * Injects headers into the passed request.
   *
   * @param {!Object} e Request event.
   * @return {!BlockingResponse} Modified headers.
   * @private
   */
  authorizeRequest_(e) {
    e.requestHeaders.push(
        {name: 'Authorization', value: 'Bearer ' + this.accessToken_});
    return /** @type {!BlockingResponse}*/ ({requestHeaders: e.requestHeaders});
  }

  /**
   * Retrieves the authorize token.
   * @return {Promise<string>} The promise with the retrieved access token.
   * @private
   */
  createTokenGetter_() {
    return new Promise((resolve, reject) => {
      if (window.IN_TEST) {
        // In test, use a dummy string as token. This must be a non-empty
        // string.
        resolve('DUMMY_ACCESS_TOKEN_FOR_TEST');
        return;
      }

      // Fetch or update the access token.
      this.delegate_.requestWebstoreAccessToken(
          /**
             @param {?string} accessToken The requested token. Null on error.
               */
          accessToken => {
            if (!accessToken) {
              reject('Error retrieving Web Store access token.');
              return;
            }
            resolve(accessToken);
          });
    });
  }

  /**
   * @return {boolean} Whether the container is in initial state, i.e. inactive.
   */
  isInInitialState() {
    return this.state_ === CWSWidgetContainer.State.UNINITIALIZED;
  }

  /**
   * Ensures that the widget container is in the state where it can properly
   * handle showing the Chrome Web Store webview.
   * @return {Promise} Resolved when the container is ready to be used.
   */
  ready() {
    return new Promise((resolve, reject) => {
      if (this.state_ !== CWSWidgetContainer.State.UNINITIALIZED) {
        reject('Invalid state.');
        return;
      }

      this.spinnerLayerController_.setAltText(
          this.delegate_.strings.LOADING_SPINNER_ALT);
      this.spinnerLayerController_.setVisible(true);

      this.metricsRecorder_.recordShowDialog();
      this.metricsRecorder_.startLoad();

      this.state_ = CWSWidgetContainer.State.GETTING_ACCESS_TOKEN;

      this.tokenGetter_.then(
          accessToken => {
            this.state_ = CWSWidgetContainer.State.ACCESS_TOKEN_READY;
            this.accessToken_ = accessToken;
            resolve();
          },
          error => {
            this.spinnerLayerController_.setVisible(false);
            this.state_ = CWSWidgetContainer.State.UNINITIALIZED;
            reject('Failed to get Web Store access token: ' + error);
          });
    });
  }

  /**
   * Initializes and starts loading the Chrome Web Store widget webview.
   * Must not be called before {@code this.ready()} is resolved.
   *
   * @param {!Object<*>} options Map of options for the dialog.
   * @param {?string} webStoreUrl Url for more results. Null if not supported.
   * @return {!Promise<CWSWidgetContainer.ResolveReason>} Resolved when app
   *     installation is done, or the installation is cancelled.
   */
  start(options, webStoreUrl) {
    return new Promise((resolve, reject) => {
      if (this.state_ !== CWSWidgetContainer.State.ACCESS_TOKEN_READY) {
        this.state_ = CWSWidgetContainer.State.INITIALIZE_FAILED_CLOSING;
        reject('Invalid state in |start|.');
        return;
      }

      if (!this.accessToken_) {
        this.state_ = CWSWidgetContainer.State.INITIALIZE_FAILED_CLOSING;
        reject('No access token.');
        return;
      }

      this.resolveStart_ = resolve;

      this.state_ = CWSWidgetContainer.State.INITIALIZING;

      this.webStoreUrl_ = webStoreUrl;
      this.options_ = options;

      this.webstoreButton_.hidden = !webStoreUrl;
      this.webstoreButton_.classList.toggle(
          'cws-widget-webstore-button', !!webStoreUrl);

      this.webview_ =
          /** @type {!WebView} */ (this.document_.createElement('webview'));
      this.webview_.id = 'cws-widget';
      this.webview_.partition = 'persist:cwswidgets';
      this.webview_.style.width = WEBVIEW_WIDTH + 'px';
      this.webview_.style.height = WEBVIEW_HEIGHT + 'px';
      this.webview_.request.onBeforeSendHeaders.addListener(
          this.authorizeRequest_.bind(this),
          /** @type {!RequestFilter}*/ ({urls: [this.widgetOrigin_ + '/*']}),
          ['blocking', 'requestHeaders']);
      this.webview_.addEventListener('newwindow', event => {
        event = /** @type {NewWindowEvent} */ (event);
        // Discard the window object and reopen in an external window.
        event.window.discard();
        window.open(event.targetUrl);
        event.preventDefault();
      });
      this.webviewContainer_.appendChild(this.webview_);

      this.spinnerLayerController_.setElementToFocusOnHide(this.webview_);
      this.spinnerLayerController_.setAltText(
          this.delegate_.strings.LOADING_SPINNER_ALT);
      this.spinnerLayerController_.setVisible(true);

      this.webviewClient_ = new CWSContainerClient(
          this.webview_, WEBVIEW_WIDTH, WEBVIEW_HEIGHT, this.widgetUrl_,
          this.widgetOrigin_, this.options_, this.delegate_);
      this.webviewClient_.addEventListener(
          CWSContainerClient.Events.LOADED, this.onWidgetLoaded_.bind(this));
      this.webviewClient_.addEventListener(
          CWSContainerClient.Events.LOAD_FAILED,
          this.onWidgetLoadFailed_.bind(this));
      this.webviewClient_.addEventListener(
          CWSContainerClient.Events.REQUEST_INSTALL,
          this.onInstallRequest_.bind(this));
      this.webviewClient_.addEventListener(
          CWSContainerClient.Events.INSTALL_DONE,
          this.onInstallDone_.bind(this));
      this.webviewClient_.load();
    });
  }

  /**
   * Called when the 'See more...' button is activated. It opens
   * {@code this.webstoreUrl_}.
   * @param {Event} e The event that activated the link. Either mouse click or
   *     key down event.
   * @private
   */
  onWebstoreLinkActivated_(e) {
    if (!this.webStoreUrl_) {
      return;
    }
    window.open(this.webStoreUrl_);
    this.state_ = CWSWidgetContainer.State.OPENING_WEBSTORE_CLOSING;
    this.reportDone_();
  }

  /**
   * Key down event handler for webstore button element. If the key is enter, it
   * activates the button.
   * @param {Event} e The event
   * @private
   */
  onWebstoreLinkKeyDown_(e) {
    if (e.keyCode !== 13 /* Enter */) {
      return;
    }
    this.onWebstoreLinkActivated_(e);
  }

  /**
   * Called when the widget is loaded successfully.
   * @param {Event} event Event.
   * @private
   */
  onWidgetLoaded_(event) {
    this.metricsRecorder_.finishLoad();
    this.metricsRecorder_.recordLoad(
        CWSWidgetContainer.MetricsRecorder.LOAD.SUCCEEDED);

    this.state_ = CWSWidgetContainer.State.INITIALIZED;

    this.spinnerLayerController_.setVisible(false);
    this.webview_.focus();
  }

  /**
   * Called when the widget is failed to load.
   * @param {Event} event Event.
   * @private
   */
  onWidgetLoadFailed_(event) {
    this.metricsRecorder_.recordLoad(
        CWSWidgetContainer.MetricsRecorder.LOAD.FAILED);

    this.spinnerLayerController_.setVisible(false);
    this.state_ = CWSWidgetContainer.State.INITIALIZE_FAILED_CLOSING;
    this.reportDone_();
  }

  /**
   * Called when the connection status is changed to offline.
   */
  onConnectionLost() {
    if (this.state_ !== CWSWidgetContainer.State.UNINITIALIZED) {
      this.state_ = CWSWidgetContainer.State.INITIALIZE_FAILED_CLOSING;
      this.reportDone_();
    }
  }

  /**
   * Called when receiving the install request from the webview client.
   * @param {Event} e Event.
   * @private
   */
  onInstallRequest_(e) {
    const itemId = e.itemId;
    this.installingItemId_ = itemId;

    this.appInstaller_ = new AppInstaller(itemId, this.delegate_);
    this.appInstaller_.install(this.onItemInstalled_.bind(this));

    this.spinnerLayerController_.setAltText(
        this.delegate_.strings.INSTALLING_SPINNER_ALT);
    this.spinnerLayerController_.setVisible(true);
    this.state_ = CWSWidgetContainer.State.INSTALLING;
  }

  /**
   * Called when the webview client receives install confirmation from the
   * Web Store widget.
   * @param {Event} e Event
   * @private
   */
  onInstallDone_(e) {
    this.spinnerLayerController_.setVisible(false);
    this.state_ = CWSWidgetContainer.State.INSTALLED_CLOSING;
    this.reportDone_();
  }

  /**
   * Called when the installation is completed from the app installer.
   * @param {AppInstaller.Result} result Result of the installation.
   * @param {string} error Detail of the error.
   * @private
   */
  onItemInstalled_(result, error) {
    const success = (result === AppInstaller.Result.SUCCESS);

    // If install succeeded, the spinner will be removed once
    // |this.webviewClient_| dispatched INSTALL_DONE event.
    if (!success) {
      this.spinnerLayerController_.setVisible(false);
    }

    this.state_ = success ?
        CWSWidgetContainer.State.WAITING_FOR_CONFIRMATION :
        CWSWidgetContainer.State.INITIALIZED;  // Back to normal state.
    this.webviewClient_.onInstallCompleted(
        success, assert(this.installingItemId_));
    this.installedItemId_ = this.installingItemId_;
    this.installingItemId_ = null;

    switch (result) {
      case AppInstaller.Result.SUCCESS:
        this.metricsRecorder_.recordInstall(
            CWSWidgetContainer.MetricsRecorder.INSTALL.SUCCEEDED);
        // Wait for the widget webview container to dispatch INSTALL_DONE.
        break;
      case AppInstaller.Result.CANCELLED:
        this.metricsRecorder_.recordInstall(
            CWSWidgetContainer.MetricsRecorder.INSTALL.CANCELLED);
        // User cancelled the installation. Do nothing.
        break;
      case AppInstaller.Result.ERROR:
        this.metricsRecorder_.recordInstall(
            CWSWidgetContainer.MetricsRecorder.INSTALL.FAILED);
        this.errorDialog_.show(
            this.delegate_.strings.INSTALLATION_FAILED_MESSAGE, null, null,
            null);
        break;
    }
  }

  /**
   * Resolves the promise returned by {@code this.start} when widget is done
   * with installing apps.
   * @private
   */
  reportDone_() {
    if (this.resolveStart_) {
      this.resolveStart_(CWSWidgetContainer.ResolveReason.DONE);
    }
    this.resolveStart_ = null;
  }

  /**
   * Finalizes the widget container state and returns the final app installation
   * result. The widget should not be used after calling this. If called before
   * promise returned by {@code this.start} is resolved, the reported result
   * will be as if the widget was cancelled.
   * @return {{result: CWSWidgetContainer.Result, installedItemId: ?string}}
   */
  finalizeAndGetResult() {
    switch (this.state_) {
      case CWSWidgetContainer.State.INSTALLING:
        // Install is being aborted. Send the failure result.
        // Cancels the install.
        if (this.webviewClient_) {
          this.webviewClient_.onInstallCompleted(
              false, assert(this.installingItemId_));
        }
        this.installingItemId_ = null;

        // Assumes closing the dialog as canceling the install.
        this.state_ = CWSWidgetContainer.State.CANCELED_CLOSING;
        break;
      case CWSWidgetContainer.State.GETTING_ACCESS_TOKEN:
      case CWSWidgetContainer.State.ACCESS_TOKEN_READY:
      case CWSWidgetContainer.State.INITIALIZING:
        this.metricsRecorder_.recordLoad(
            CWSWidgetContainer.MetricsRecorder.LOAD.CANCELLED);
        this.state_ = CWSWidgetContainer.State.CANCELED_CLOSING;
        break;
      case CWSWidgetContainer.State.WAITING_FOR_CONFIRMATION:
        // This can happen if the dialog is closed by the user before Web Store
        // widget replies with 'after_install'.
        // Consider this success, as the app has actually been installed.
        // TODO(tbarzic): Should the app be uninstalled in this case?
        this.state_ = CWSWidgetContainer.State.INSTALLED_CLOSING;
        break;
      case CWSWidgetContainer.State.INSTALLED_CLOSING:
      case CWSWidgetContainer.State.INITIALIZE_FAILED_CLOSING:
      case CWSWidgetContainer.State.OPENING_WEBSTORE_CLOSING:
        // Do nothing.
        break;
      case CWSWidgetContainer.State.INITIALIZED:
        this.state_ = CWSWidgetContainer.State.CANCELED_CLOSING;
        break;
      default:
        this.state_ = CWSWidgetContainer.State.CANCELED_CLOSING;
        console.error('Invalid state.');
    }

    let result;
    switch (this.state_) {
      case CWSWidgetContainer.State.INSTALLED_CLOSING:
        result = CWSWidgetContainer.Result.INSTALL_SUCCESSFUL;
        this.metricsRecorder_.recordCloseDialog(
            CWSWidgetContainer.MetricsRecorder.CLOSE_DIALOG.ITEM_INSTALLED);
        break;
      case CWSWidgetContainer.State.INITIALIZE_FAILED_CLOSING:
        result = CWSWidgetContainer.Result.FAILED;
        break;
      case CWSWidgetContainer.State.CANCELED_CLOSING:
        result = CWSWidgetContainer.Result.USER_CANCEL;
        this.metricsRecorder_.recordCloseDialog(
            CWSWidgetContainer.MetricsRecorder.CLOSE_DIALOG.USER_CANCELLED);
        break;
      case CWSWidgetContainer.State.OPENING_WEBSTORE_CLOSING:
        result = CWSWidgetContainer.Result.WEBSTORE_LINK_OPENED;
        this.metricsRecorder_.recordCloseDialog(
            CWSWidgetContainer.MetricsRecorder.CLOSE_DIALOG
                .WEBSTORE_LINK_OPENED);
        break;
      default:
        result = CWSWidgetContainer.Result.USER_CANCEL;
        this.metricsRecorder_.recordCloseDialog(
            CWSWidgetContainer.MetricsRecorder.CLOSE_DIALOG.UNKNOWN_ERROR);
    }

    this.state_ = CWSWidgetContainer.State.UNINITIALIZED;

    this.reset_();

    return {result: result, installedItemId: this.installedItemId_};
  }

  /**
   * Resets the widget.
   * @private
   */
  reset_() {
    if (this.state_ !== CWSWidgetContainer.State.UNINITIALIZED) {
      console.error('Widget reset before its state was finalized.');
    }

    if (this.resolveStart_) {
      this.resolveStart_(CWSWidgetContainer.ResolveReason.RESET);
      this.resolveStart_ = null;
    }

    this.spinnerLayerController_.reset();

    if (this.webviewClient_) {
      this.webviewClient_.dispose();
      this.webviewClient_ = null;
    }

    if (this.webview_) {
      this.webviewContainer_.removeChild(this.webview_);
      this.webview_ = null;
    }

    if (this.appInstaller_) {
      this.appInstaller_.cancel();
      this.appInstaller_ = null;
    }

    this.options_ = null;

    if (this.errorDialog_.shown()) {
      this.errorDialog_.hide();
    }
  }
}

/**
 * @enum {string}
 * @private
 */
CWSWidgetContainer.State = {
  UNINITIALIZED: 'CWSWidgetContainer.State.UNINITIALIZED',
  GETTING_ACCESS_TOKEN: 'CWSWidgetContainer.State.GETTING_ACCESS_TOKEN',
  ACCESS_TOKEN_READY: 'CWSWidgetContainer.State.ACCESS_TOKEN_READY',
  INITIALIZING: 'CWSWidgetContainer.State.INITIALIZING',
  INITIALIZE_FAILED_CLOSING:
      'CWSWidgetContainer.State.INITIALIZE_FAILED_CLOSING',
  INITIALIZED: 'CWSWidgetContainer.State.INITIALIZED',
  INSTALLING: 'CWSWidgetContainer.State.INSTALLING',
  WAITING_FOR_CONFIRMATION: 'CWSWidgetContainer.State.WAITING_FOR_CONFIRMATION',
  INSTALLED_CLOSING: 'CWSWidgetContainer.State.INSTALLED_CLOSING',
  OPENING_WEBSTORE_CLOSING: 'CWSWidgetContainer.State.OPENING_WEBSTORE_CLOSING',
  CANCELED_CLOSING: 'CWSWidgetContainer.State.CANCELED_CLOSING'
};
Object.freeze(CWSWidgetContainer.State);

/**
 * @enum {string}
 * @const
 */
CWSWidgetContainer.Result = {
  /** Install is done. The install app should be opened. */
  INSTALL_SUCCESSFUL: 'CWSWidgetContainer.Result.INSTALL_SUCCESSFUL',
  /** User cancelled the suggest app dialog. No message should be shown. */
  USER_CANCEL: 'CWSWidgetContainer.Result.USER_CANCEL',
  /** User clicked the link to web store so the dialog is closed. */
  WEBSTORE_LINK_OPENED: 'CWSWidgetContainer.Result.WEBSTORE_LINK_OPENED',
  /** Failed to load the widget. Error message should be shown. */
  FAILED: 'CWSWidgetContainer.Result.FAILED'
};
Object.freeze(CWSWidgetContainer.Result);

/**
 * The reason due to which the container is resolving {@code this.start}
 * promise.
 * @enum {string}
 */
CWSWidgetContainer.ResolveReason = {
  /** The widget container ended up in its final state. */
  DONE: 'CWSWidgetContainer.ResolveReason.DONE',
  /** The widget container is being reset. */
  RESET: 'CWSWidgetContainer.CloserReason.RESET'
};
Object.freeze(CWSWidgetContainer.ResolveReason);


CWSWidgetContainer.SpinnerLayerController = class {
  /**
   * Controls showing and hiding spinner layer.
   * @param {!Element} spinnerLayer The spinner layer element.
   */
  constructor(spinnerLayer) {
    /** @private {!Element} */
    this.spinnerLayer_ = spinnerLayer;

    /** @private {boolean} */
    this.visible_ = false;

    /**
     * Set only if spinner is transitioning between visible and hidden states.
     * Calling the function clears event handlers set for handling the
     * transition, and updates spinner layer class list to its final state.
     * @type {?function()}
     * @private
     */
    this.clearTransition_ = null;

    /**
     * Reference to the timeout set to ensure {@code this.clearTransition_} gets
     * called even if 'transitionend' event does not fire.
     * @type {?number}
     * @private
     */
    this.clearTransitionTimeout_ = null;

    /**
     * Element to be focused when the layer is hidden.
     * @type {Element}
     * @private
     */
    this.focusOnHide_ = null;

    spinnerLayer.tabIndex = -1;

    // Prevent default Tab key handling in order to prevent the widget from
    // taking the focus while the spinner layer is active.
    // NOTE: This assumes that there are no elements allowed to become active
    // while the spinner is shown. Something smarter would be needed if this
    // assumption becomes invalid.
    spinnerLayer.addEventListener('keydown', this.handleKeyDown_.bind(this));
  }

  /**
   * Sets element to be focused when the layer is hidden.
   * @param {!Element} el
   */
  setElementToFocusOnHide(el) {
    this.focusOnHide_ = el;
  }

  /**
   * Prevents default Tab key handling in order to prevent spinner layer from
   * losing focus.
   * @param {Event} e The key down event.
   * @private
   */
  handleKeyDown_(e) {
    if (!this.visible_) {
      return;
    }
    if (e.keyCode === 9 /* Tab */) {
      e.preventDefault();
    }
  }

  /**
   * Resets the spinner layer controllers state, and makes sure the spinner
   * layre gets hidden.
   */
  reset() {
    this.visible_ = false;
    this.focusOnHide_ = null;
    if (this.clearTransition_) {
      this.clearTransition_();
    }
  }

  /**
   * Sets alt text for the spinner layer.
   * @param {string} text
   */
  setAltText(text) {
    this.spinnerLayer_.setAttribute('aria-label', text);
  }

  /**
   * Shows or hides the spinner layer and handles the layer's opacity
   * transition.
   * @param {boolean} visible Whether the layer should become visible.
   */
  setVisible(visible) {
    if (this.visible_ === visible) {
      return;
    }

    if (this.clearTransition_) {
      this.clearTransition_();
    }

    this.visible_ = visible;

    // Spinner should be shown during transition.
    this.spinnerLayer_.classList.toggle('cws-widget-show-spinner', true);

    if (this.visible_) {
      this.spinnerLayer_.focus();
    } else if (this.focusOnHide_) {
      this.focusOnHide_.focus();
    }

    if (!this.visible_) {
      this.spinnerLayer_.classList.add('cws-widget-hiding-spinner');
    }

    this.clearTransition_ = () => {
      if (this.clearTransitionTimeout_) {
        clearTimeout(this.clearTransitionTimeout_);
      }
      this.clearTransitionTimeout_ = null;

      this.spinnerLayer_.removeEventListener(
          'transitionend', this.clearTransition_);
      this.clearTransition_ = null;

      if (!this.visible_) {
        this.spinnerLayer_.classList.remove('cws-widget-hiding-spinner');
        this.spinnerLayer_.classList.remove('cws-widget-show-spinner');
      }
    };

    this.spinnerLayer_.addEventListener('transitionend', this.clearTransition_);

    // Ensure the transition state gets cleared, even if transitionend is not
    // fired.
    this.clearTransitionTimeout_ = setTimeout(() => {
      this.clearTransitionTimeout_ = null;
      this.clearTransition_();
    }, 550 /* ms */);
  }
};

/**
 * Utility methods and constants to record histograms.
 */
CWSWidgetContainer.MetricsRecorder = class {
  /**
   * @param {!CWSWidgetContainerMetricsImpl} metricsImpl
   */
  constructor(metricsImpl) {
    /** @private {!CWSWidgetContainerMetricsImpl} */
    this.metricsImpl_ = metricsImpl;
  }


  /**
   * @param {number} result Result of load, which must be defined in
   *     CWSWidgetContainer.MetricsRecorder.LOAD.
   */
  recordLoad(result) {
    if (0 <= result && result < 3) {
      this.metricsImpl_.recordEnum('Load', result, 3);
    }
  }

  /**
   * @param {number} reason Reason of closing dialog, which must be defined in
   *     CWSWidgetContainer.MetricsRecorder.CLOSE_DIALOG.
   */
  recordCloseDialog(reason) {
    if (0 <= reason && reason < 4) {
      this.metricsImpl_.recordEnum('CloseDialog', reason, 4);
    }
  }

  /**
   * @param {number} result Result of installation, which must be defined in
   *     CWSWidgetContainer.MetricsRecorder.INSTALL.
   */
  recordInstall(result) {
    if (0 <= result && result < 3) {
      this.metricsImpl_.recordEnum('Install', result, 3);
    }
  }

  recordShowDialog() {
    this.metricsImpl_.recordUserAction('ShowDialog');
  }

  startLoad() {
    this.metricsImpl_.startInterval('LoadTime');
  }

  finishLoad() {
    this.metricsImpl_.recordInterval('LoadTime');
  }
};

/**
 * @enum {number}
 * @const
 */
CWSWidgetContainer.MetricsRecorder.LOAD = {
  SUCCEEDED: 0,
  CANCELLED: 1,
  FAILED: 2,
};

/**
 * @enum {number}
 * @const
 */
CWSWidgetContainer.MetricsRecorder.CLOSE_DIALOG = {
  UNKNOWN_ERROR: 0,
  ITEM_INSTALLED: 1,
  USER_CANCELLED: 2,
  WEBSTORE_LINK_OPENED: 3,
};

/**
 * @enum {number}
 * @const
 */
CWSWidgetContainer.MetricsRecorder.INSTALL = {
  SUCCEEDED: 0,
  CANCELLED: 1,
  FAILED: 2,
};
