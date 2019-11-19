// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Carrier Provisioning subpage in Cellular Setup flow. This element contains a
 * webview element that loads the carrier's provisioning portal. It also has an
 * error state that displays a message for errors that may happen during this
 * step.
 */
Polymer({
  is: 'provisioning-page',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Whether error state should be shown.
     * @type {boolean}
     */
    showError: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /**
     * Metadata used to open carrier provisioning portal. Expected to start as
     * null, then change to a valid object.
     * @type {?chromeos.cellularSetup.mojom.CellularMetadata}
     */
    cellularMetadata: {
      type: Object,
      value: null,
      observer: 'onCellularMetadataChanged_',
    },

    /**
     * Whether the carrier portal has completed being loaded.
     * @private {boolean}
     */
    hasCarrierPortalLoaded_: {
      type: Boolean,
      value: false,
    },

    /**
     * The last carrier name provided via |cellularMetadata|.
     * @private {string}
     */
    carrierName_: {
      type: String,
      value: '',
    },
  },

  /**
   * @return {string}
   * @private
   */
  getPageTitle_: function() {
    if (this.showError) {
      return this.i18n('provisioningPageErrorTitle', this.carrierName_);
    }
    if (this.hasCarrierPortalLoaded_) {
      return this.i18n('provisioningPageActiveTitle');
    }
    return this.i18n('provisioningPageLoadingTitle', this.carrierName_);
  },

  /**
   * @return {string}
   * @private
   */
  getPageMessage_: function() {
    if (this.showError) {
      return this.i18n('provisioningPageErrorMessage', this.carrierName_);
    }
    return '';
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSpinner_: function() {
    return !this.showError && !this.hasCarrierPortalLoaded_;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPortal_: function() {
    return !this.showError && this.hasCarrierPortalLoaded_;
  },

  /**
   * @return {?WebView}
   * @private
   */
  getPortalWebview: function() {
    return /** @type {?WebView} */ (this.$$('webview'));
  },

  /** @private */
  onCellularMetadataChanged_: function() {
    // Once |cellularMetadata| has been set, load the carrier provisioning page.
    if (this.cellularMetadata) {
      this.carrierName_ = this.cellularMetadata.carrier;
      this.loadPortal_();
      return;
    }

    // If |cellularMetadata| is now null, the page should be reset so that a new
    // attempt can begin.
    this.resetPage_();
  },

  /** @private */
  loadPortal_: function() {
    assert(!!this.cellularMetadata);
    assert(!this.getPortalWebview());

    const portalWebview =
        /** @type {!WebView} */ (document.createElement('webview'));
    this.$.portalContainer.appendChild(portalWebview);

    portalWebview.addEventListener(
        'loadabort', this.onPortalLoadAbort_.bind(this));
    portalWebview.addEventListener(
        'loadstop', this.onPortalLoadStop_.bind(this));
    window.addEventListener('message', this.onMessageReceived_.bind(this));

    // Setting a <webview>'s "src" attribute triggers a GET request, but some
    // carrier portals require a POST request instead. If data is provided for a
    // POST request body, use a utility function to load the webview.
    if (this.cellularMetadata.paymentPostData) {
      webviewPost.util.postDeviceDataToWebview(
          portalWebview, this.cellularMetadata.paymentUrl.url,
          this.cellularMetadata.paymentPostData);
      return;
    }

    // Otherwise, use a normal GET request by specifying the "src".
    portalWebview.src = this.cellularMetadata.paymentUrl.url;
  },

  /** @private */
  resetPage_: function() {
    this.hasCarrierPortalLoaded_ = false;

    // Remove the portal from the DOM if it exists.
    const portalWebview = this.getPortalWebview();
    if (portalWebview) {
      portalWebview.remove();
    }
  },

  /** @private */
  onPortalLoadAbort_: function(event) {
    this.showError = true;
  },

  /** @private */
  onPortalLoadStop_: function() {
    if (this.hasCarrierPortalLoaded_) {
      return;
    }

    this.hasCarrierPortalLoaded_ = true;
    this.fire('carrier-portal-loaded');

    // When the portal loads, it expects to receive a message from this frame
    // alerting it that loading has completed successfully.
    this.getPortalWebview().contentWindow.postMessage(
        {msg: 'loadedInWebview'}, this.cellularMetadata.paymentUrl.url);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onMessageReceived_: function(event) {
    const messageType = /** @type {string} */ (event.data.type);
    const status = /** @type {string} */ (event.data.status);

    // The <webview> requested information about this device. Reply by posting a
    // message back to it.
    if (messageType == 'requestDeviceInfoMsg') {
      this.getPortalWebview().contentWindow.postMessage(
          {
            carrier: this.cellularMetadata.carrier,
            MEID: this.cellularMetadata.meid,
            IMEI: this.cellularMetadata.imei,
            MDN: this.cellularMetadata.mdn
          },
          this.cellularMetadata.paymentUrl.url);
      return;
    }

    // The <webview> provided an update on the status of the activation attempt.
    if (messageType == 'reportTransactionStatusMsg') {
      const success = status == 'ok';
      this.fire('on-carrier-portal-result', success);
      return;
    }
  },
});
