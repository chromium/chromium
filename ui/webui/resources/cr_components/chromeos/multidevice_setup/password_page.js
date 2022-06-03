// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'password-page',

  behaviors: [
    UiPageContainerBehavior,
  ],

  properties: {
    /**
     * Whether forward button should be disabled. In this context, the forward
     * button should be disabled if the user has not entered a password or if
     * the user has submitted an incorrect password and has not yet edited it.
     * @type {boolean}
     */
    forwardButtonDisabled: {
      type: Boolean,
      computed: 'shouldForwardButtonBeDisabled_(' +
          'inputValue_, passwordInvalid_, waitingForPasswordCheck_)',
      notify: true,
    },

    /** Overridden from UiPageContainerBehavior. */
    forwardButtonTextId: {
      type: String,
      value: 'done',
    },

    /** Overridden from UiPageContainerBehavior. */
    cancelButtonTextId: {
      type: String,
      value: 'cancel',
    },

    /** Overridden from UiPageContainerBehavior. */
    backwardButtonTextId: {
      type: String,
      value: 'back',
    },

    /**
     * Authentication token; retrieved using the quickUnlockPrivate API.
     * @type {string}
     */
    authToken: {
      type: String,
      value: '',
      notify: true,
    },

    /** @private {string} */
    profilePhotoUrl_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    email_: {
      type: String,
      value: '',
    },

    /** @private {!QuickUnlockPrivate} */
    quickUnlockPrivate_: {
      type: Object,
      value: chrome.quickUnlockPrivate,
    },

    /** @private {string} */
    inputValue_: {
      type: String,
      value: '',
      observer: 'onInputValueChange_',
    },

    /** @private {boolean} */
    passwordInvalid_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    waitingForPasswordCheck_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {?multidevice_setup.BrowserProxy} */
  browserProxy_: null,

  clearPasswordTextInput() {
    this.$.passwordInput.value = '';
  },

  focusPasswordTextInput() {
    this.$.passwordInput.focus();
  },

  /** @override */
  created() {
    this.browserProxy_ = multidevice_setup.BrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.browserProxy_.getProfileInfo().then((profileInfo) => {
      this.profilePhotoUrl_ = profileInfo.profilePhotoUrl;
      this.email_ = profileInfo.email;
    });
  },

  /** Overridden from UiPageContainerBehavior. */
  getCanNavigateToNextPage() {
    return new Promise((resolve) => {
      if (this.waitingForPasswordCheck_) {
        resolve(false /* canNavigate */);
        return;
      }
      this.waitingForPasswordCheck_ = true;
      this.quickUnlockPrivate_.getAuthToken(this.inputValue_, (tokenInfo) => {
        this.waitingForPasswordCheck_ = false;
        if (chrome.runtime.lastError) {
          this.passwordInvalid_ = true;
          // Select the password text if the user entered an incorrect password.
          this.$.passwordInput.select();
          resolve(false /* canNavigate */);
          return;
        }
        this.authToken = tokenInfo.token;
        this.passwordInvalid_ = false;
        resolve(true /* canNavigate */);
      });
    });
  },

  /** @private */
  onInputValueChange_() {
    this.passwordInvalid_ = false;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onInputKeypress_(e) {
    // We are only listening for the user trying to enter their password.
    if (e.key !== 'Enter') {
      return;
    }

    this.fire('user-submitted-password');
  },

  /**
   * @return {boolean} Whether the forward button should be disabled.
   * @private
   */
  shouldForwardButtonBeDisabled_() {
    return this.passwordInvalid_ || !this.inputValue_ ||
        this.waitingForPasswordCheck_;
  },
});
