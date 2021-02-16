// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @enum {number} */
/* #export */ const LoadingPageState = {
  LOADING: 1,
  SIM_DETECT_ERROR: 2,
  CELLULAR_DISCONNECT_WARNING: 3,
};

/**
 * Loading subpage in Cellular Setup flow. This element contains image
 * asset and description to indicate that a SIM detection or eSIM profiles
 * loading is in progress. It can show a 'detecting sim' error or a 'cellular
 * disconnection' warning depending on its state.
 */
Polymer({
  is: 'setup-loading-page',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!cellular_setup.CellularSetupDelegate} */
    delegate: Object,

    /** @type {!LoadingPageState} */
    state: {
      type: Object,
      value: LoadingPageState.LOADING,
    },

    /**
     * Message displayed with spinner when in LOADING state.
     */
    loadingMessage: {
      type: String,
      value: '',
    },
  },

  /**
   * @param {LoadingPageState} state
   * @return {?string}
   * @private
   */
  getTitle_(state) {
    if (this.delegate.shouldShowPageTitle() &&
        state === LoadingPageState.SIM_DETECT_ERROR) {
      return this.i18n('simDetectPageErrorTitle');
    }
    return null;
  },

  /**
   * @param {LoadingPageState} state
   * @return {string}
   * @private
   */
  getMessage_(state) {
    switch (state) {
      case LoadingPageState.SIM_DETECT_ERROR:
        return this.i18n('simDetectPageErrorMessage');
      case LoadingPageState.CELLULAR_DISCONNECT_WARNING:
        return this.i18n('eSimConnectionWarning');
      case LoadingPageState.LOADING:
        return '';
      default:
        assertNotReached();
    }
  },

  /**
   * @param {LoadingPageState} state
   * @return {string}
   * @private
   */
  getMessageIcon_(state) {
    return state === LoadingPageState.CELLULAR_DISCONNECT_WARNING ? 'warning' :
                                                                    '';
  },

  /**
   * @param {LoadingPageState} state
   * @return {boolean}
   * @private
   */
  shouldShowSimDetectError_(state) {
    return state === LoadingPageState.SIM_DETECT_ERROR;
  },
});
