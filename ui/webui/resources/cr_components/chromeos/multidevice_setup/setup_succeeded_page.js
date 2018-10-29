// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('multidevice_setup');

Polymer({
  is: 'setup-succeeded-page',

  properties: {
    /** Overridden from UiPageContainerBehavior. */
    forwardButtonTextId: {
      type: String,
      value: 'done',
    },

    /** Overridden from UiPageContainerBehavior. */
    headerId: {
      type: String,
      value: 'setupSucceededPageHeader',
    },

    /** Overridden from UiPageContainerBehavior. */
    messageId: {
      type: String,
      value: 'setupSucceededPageMessage',
    },
  },

  behaviors: [
    UiPageContainerBehavior,
  ],

  /** @private {?multidevice_setup.BrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = multidevice_setup.BrowserProxyImpl.getInstance();
  },

  /** @private */
  openSettings_: function() {
    this.browserProxy_.openMultiDeviceSettings();
  },

  /** @private */
  onSettingsLinkClicked_: function() {
    this.openSettings_();
    this.fire('setup-exited');
  },

  /** @override */
  ready: function() {
    let linkElement = this.$$('#settings-link');
    linkElement.setAttribute('href', '#');
    linkElement.addEventListener('click', () => this.onSettingsLinkClicked_());
  },
});
