// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {string}
 */
const SRC_SET_URL_1_LIGHT =
    'chrome://resources/cr_components/chromeos/multidevice_setup/all_set_1x_light.svg';

/**
 * @type {string}
 */
const SRC_SET_URL_2_LIGHT =
    'chrome://resources/cr_components/chromeos/multidevice_setup/all_set_2x_light.svg';

/**
 * @type {string}
 */
const SRC_SET_URL_1_DARK =
    'chrome://resources/cr_components/chromeos/multidevice_setup/all_set_1x_dark.svg';

/**
 * @type {string}
 */
const SRC_SET_URL_2_DARK =
    'chrome://resources/cr_components/chromeos/multidevice_setup/all_set_2x_dark.svg';

Polymer({
  is: 'setup-succeeded-page',

  properties: {
    /** Overridden from UiPageContainerBehavior. */
    forwardButtonTextId: {
      type: String,
      value: 'done',
    },

    /**
     * Whether the multidevice success page is being rendered in dark mode.
     * @private {boolean}
     */
    isDarkModeActive_: {
      type: Boolean,
      value: false,
    },
  },

  behaviors: [UiPageContainerBehavior],

  /** @private {?multidevice_setup.BrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = multidevice_setup.BrowserProxyImpl.getInstance();
  },

  /** @private */
  openSettings_() {
    this.browserProxy_.openMultiDeviceSettings();
  },

  /** @private */
  onSettingsLinkClicked_() {
    this.openSettings_();
    this.fire('setup-exited');
  },

  /** @private */
  getMessageHtml_() {
    return this.i18nAdvanced('setupSucceededPageMessage', {attrs: ['id']});
  },

  /** @override */
  ready() {
    const linkElement = this.$$('#settings-link');
    linkElement.setAttribute('href', '#');
    linkElement.addEventListener('click', () => this.onSettingsLinkClicked_());
  },

  /**
   * Returns source set for images based on if the page is rendered in dark
   * mode.
   * @return {string}
   * @private
   */
  getImageSrcSet_() {
    return this.isDarkModeActive_ ?
        SRC_SET_URL_1_DARK + ' 1x, ' + SRC_SET_URL_2_DARK + ' 2x' :
        SRC_SET_URL_1_LIGHT + ' 1x, ' + SRC_SET_URL_2_LIGHT + ' 2x';
  },
});
