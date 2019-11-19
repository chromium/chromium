// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Account picker screen implementation.
 */

login.createScreen('AccountPickerScreen', 'account-picker', function() {
  /**
   * Maximum number of offline login failures before online login.
   * @type {number}
   * @const
   */
  var MAX_LOGIN_ATTEMPTS_IN_POD = 3;

  /**
   * Time after which the sign-in error bubble should be hidden if it is
   * overlayed over the detachable base change warning bubble (to ensure that
   * the detachable base warning is not obscured indefinitely).
   * @const {number}
   */
  var SIGNIN_ERROR_OVER_DETACHABLE_BASE_WARNING_TIMEOUT_MS = 5000;

  return {
    EXTERNAL_API: [
      'loadUsers',
      'updateUserImage',
      'setCapsLockState',
      'removeUser',
      'showBannerMessage',
      'showUserPodCustomIcon',
      'hideUserPodCustomIcon',
      'setUserPodFingerprintIcon',
      'removeUserPodFingerprintIcon',
      'selectPodForDetachableBaseWarningBubble',
      'setPinEnabledForUser',
      'setAuthType',
      'setTabletModeState',
      'setDemoModeState',
      'setPublicSessionDisplayName',
      'setPublicSessionLocales',
      'setPublicSessionKeyboardLayouts',
      'setOverlayColors',
      'togglePodBackground',
    ],

    preferredWidth_: 0,
    preferredHeight_: 0,

    // Whether this screen is shown for the first time.
    firstShown_: true,

    // Whether this screen is currently being shown.
    showing_: false,

    /** @override */
    decorate: function() {
      login.PodRow.decorate($('pod-row'));
    },

    /** @override */
    getPreferredSize: function() {
      return {width: this.preferredWidth_, height: this.preferredHeight_};
    },

    /** @override */
    onWindowResize: function() {
      $('pod-row').onWindowResize();
    },

    /**
     * Sets preferred size for account picker screen.
     */
    setPreferredSize: function(width, height) {
      this.preferredWidth_ = width;
      this.preferredHeight_ = height;
    },

    /**
     * Sets login screen overlay colors based on colors extracted from the
     * wallpaper.
     * @param {string} maskColor Color for the gradient mask.
     * @param {string} scrollColor Color for the small pods container.
     */
    setOverlayColors: function(maskColor, scrollColor) {
      $('pod-row').setOverlayColors(maskColor, scrollColor);
    },

    /**
     * Toggles the background behind user pods.
     * @param {boolean} showPodBackground Whether to add background behind user
     *     pods.
     */
    togglePodBackground: function(showPodBackground) {
      $('pod-row').togglePodBackground(showPodBackground);
    },

    /* Cancel user adding if ESC was pressed.
     */
    cancel: function() {
      if (Oobe.getInstance().displayType == DISPLAY_TYPE.USER_ADDING)
        chrome.send('cancelUserAdding');
    },

    /**
     * Event handler that is invoked just after the frame is shown.
     * @param {string} data Screen init payload.
     */
    onAfterShow: function(data) {
      $('pod-row').handleAfterShow();
    },

    /**
     * Event handler that is invoked just before the frame is shown.
     * @param {string} data Screen init payload.
     */
    onBeforeShow: function(data) {
      this.showing_ = true;

      chrome.send('loginUIStateChanged', ['account-picker', true]);

      chrome.send('hideCaptivePortal');
      var podRow = $('pod-row');
      podRow.signinUIState = SIGNIN_UI_STATE.ACCOUNT_PICKER;
      podRow.handleBeforeShow();

      // In case of the preselected pod onShow will be called once pod
      // receives focus.
      if (!podRow.preselectedPod)
        this.onShow();
    },

    /**
     * Event handler invoked when the page is shown and ready.
     */
    onShow: function() {
      if (!this.showing_) {
        // This method may be called asynchronously when the pod row finishes
        // initializing. However, at that point, the screen may have been hidden
        // again already. If that happens, ignore the onShow() call.
        return;
      }
      chrome.send('getTabletModeState');
      chrome.send('getDemoModeState');
      if (!this.firstShown_)
        return;
      this.firstShown_ = false;

      // Ensure that login is actually visible.
      window.requestAnimationFrame(function() {
        chrome.send('accountPickerReady');
        chrome.send('loginVisible', ['account-picker']);
      });
    },

    /**
     * Event handler that is invoked just before the frame is hidden.
     */
    onBeforeHide: function() {
      $('pod-row').clearFocusedPod();
      $('bubble-persistent').hide();
      this.showing_ = false;
      chrome.send('loginUIStateChanged', ['account-picker', false]);
      var podRow = $('pod-row');
      podRow.signinUIState = SIGNIN_UI_STATE.HIDDEN;
      podRow.handleHide();
    },

    /**
     * Shows sign-in error bubble.
     * @param {number} loginAttempts Number of login attemps tried.
     * @param {HTMLElement} error Error to show in bubble.
     */
    showErrorBubble: function(loginAttempts, error) {
      var activatedPod = $('pod-row').activatedPod;
      if (!activatedPod) {
        $('bubble').showContentForElement($('pod-row'),
                                          cr.ui.Bubble.Attachment.RIGHT,
                                          error);
        return;
      }
      // Show web authentication if this is not a supervised user.
      if (loginAttempts > MAX_LOGIN_ATTEMPTS_IN_POD &&
          !activatedPod.user.supervisedUser) {
        chrome.send('maxIncorrectPasswordAttempts',
            [activatedPod.user.emailAddress]);
        activatedPod.showSigninUI();
      } else {
        if (loginAttempts == 1) {
          chrome.send('firstIncorrectPasswordAttempt',
              [activatedPod.user.emailAddress]);
        }
        // Update the pod row display if incorrect password.
        $('pod-row').setFocusedPodErrorDisplay(true);

        // If a warning that the detachable base is different than the one
        // previously used by the user is shown for the pod, make sure that the
        // sign-in error gets hidden reasonably soon.
        // If the detachable base was changed maliciously while the user was
        // away, the attacker might attempt to use the sign-in error but to
        // obscure the detachable base warning hoping that the user will miss it
        // when they get back to the device.
        var timeout = activatedPod.showingDetachableBaseWarningBubble() ?
            SIGNIN_ERROR_OVER_DETACHABLE_BASE_WARNING_TIMEOUT_MS :
            undefined;
        activatedPod.showBubble(error, {timeout: timeout});
      }
    },

    /**
     * Ensures that a user pod is selected and focused, and thus ready to show a
     * warning bubble for detachable base change. This is needed for two
     * reasons:
     *   1. The detachable base state is associated with a user, so a user pod
     *      has to be selected in order to know for which user the detachable
     *      base state should be considered (e.g. there might be two large user
     *      pods in the account picker).
     *   2. The warning bubble is attached to the pod's auth element, which is
     *      only shown if the pod is focused. The bubble anchor should be
     *      visible in order to properly calculate the bubble position.
     */
    selectPodForDetachableBaseWarningBubble: function() {
      $('pod-row').maybePreselectPod();
    },

    /**
     * Shows a persistent bubble warning to the user that the current detachable
     * base is different than the one they were last using, and that it might
     * not be trusted.
     *
     * @param {string} username The username of the user under whose user pod
     *     the warning should be displayed.
     * @param {HTMLElement} content The warning bubble content.
     */
    showDetachableBaseWarningBubble: function(username, content) {
      var podRow = $('pod-row');
      var pod = podRow.pods.find(pod => pod.user.username == username);
      if (pod)
        pod.showDetachableBaseWarningBubble(content);
    },

    /**
     * Hides the detachable base warning for the user.
     *
     * @param {string} username The username that identifies the user pod from
     *     under which the detachable base warning bubble should be removed.
     */
    hideDetachableBaseWarningBubble: function(username) {
      var pod = $('pod-row').pods.find(pod => pod.user.username == username);
      if (pod)
        pod.hideDetachableBaseWarningBubble();
    },

    /**
     * Loads given users in pod row.
     * @param {array} users Array of user.
     */
    loadUsers: function(users) {
      $('pod-row').loadPods(users);
    },

    /**
     * Updates current image of a user.
     * @param {string} username User for which to update the image.
     */
    updateUserImage: function(username) {
      $('pod-row').updateUserImage(username);
    },

    /**
     * Updates Caps Lock state (for Caps Lock hint in password input field).
     * @param {boolean} enabled Whether Caps Lock is on.
     */
    setCapsLockState: function(enabled) {
      $('pod-row').classList.toggle('capslock-on', enabled);
    },

    /**
     * Remove given user from pod row if it is there.
     * @param {string} user name.
     */
    removeUser: function(username) {
      $('pod-row').removeUserPod(username);
    },

    /**
     * Displays a banner containing |message|. If the banner is already present
     * this function updates the message in the banner. This function is used
     * by the chrome.screenlockPrivate.showMessage API.
     * @param {string} message Text to be displayed or empty to hide the banner.
     * @param {boolean} isWarning True if the given message is a warning.
     */
    showBannerMessage: function(message, isWarning) {
      $('pod-row').showBannerMessage(message, isWarning);
    },

    /**
     * Shows a custom icon in the user pod of |username|. This function
     * is used by the chrome.screenlockPrivate API.
     * @param {string} username Username of pod to add button
     * @param {!{id: !string,
     *           hardlockOnClick: boolean,
     *           isTrialRun: boolean,
     *           tooltip: ({text: string, autoshow: boolean} | undefined)}} icon
     *     The icon parameters.
     */
    showUserPodCustomIcon: function(username, icon) {
      $('pod-row').showUserPodCustomIcon(username, icon);
    },

    /**
     * Hides the custom icon in the user pod of |username| added by
     * showUserPodCustomIcon(). This function is used by the
     * chrome.screenlockPrivate API.
     * @param {string} username Username of pod to remove button
     */
    hideUserPodCustomIcon: function(username) {
      $('pod-row').hideUserPodCustomIcon(username);
    },

    /**
     * Set a fingerprint icon in the user pod of |username|.
     * @param {string} username Username of the selected user
     * @param {number} state Fingerprint unlock state
     */
    setUserPodFingerprintIcon: function(username, state) {
      $('pod-row').setUserPodFingerprintIcon(username, state);
    },

    /**
     * Removes the fingerprint icon in the user pod of |username|.
     * @param {string} username Username of the selected user.
     */
    removeUserPodFingerprintIcon: function(username) {
      $('pod-row').removeUserPodFingerprintIcon(username);
    },

    /**
     * Sets the authentication type used to authenticate the user.
     * @param {string} username Username of selected user
     * @param {number} authType Authentication type, must be a valid value in
     *                          the AUTH_TYPE enum in chromeos_user_pod_row.js.
     * @param {string} value The initial value to use for authentication.
     */
    setAuthType: function(username, authType, value) {
      $('pod-row').setAuthType(username, authType, value);
    },

    /**
     * Sets the state of tablet mode.
     * @param {boolean} isTabletModeEnabled true if the mode is on.
     */
    setTabletModeState: function(isTabletModeEnabled) {
      $('pod-row').setTabletModeState(isTabletModeEnabled);
    },

    /**
     * Sets whether the device is in demo mode.
     * @param {boolean} isDeviceInDemoMode true if the device is in demo mode.
     */
    setDemoModeState: function(isDeviceInDemoMode) {
      $('pod-row').setDemoModeState(isDeviceInDemoMode);
    },

    /**
     * Enables or disables the pin keyboard for the given user. This may change
     * pin keyboard visibility.
     * @param {!string} user
     * @param {boolean} enabled
     */
    setPinEnabledForUser: function(user, enabled) {
      $('pod-row').setPinEnabled(user, enabled);
    },

    /**
     * Updates the display name shown on a public session pod.
     * @param {string} userID The user ID of the public session
     * @param {string} displayName The new display name
     */
    setPublicSessionDisplayName: function(userID, displayName) {
      $('pod-row').setPublicSessionDisplayName(userID, displayName);
    },

    /**
     * Updates the list of locales available for a public session.
     * @param {string} userID The user ID of the public session
     * @param {!Object} locales The list of available locales
     * @param {string} defaultLocale The locale to select by default
     * @param {boolean} multipleRecommendedLocales Whether |locales| contains
     *     two or more recommended locales
     */
    setPublicSessionLocales: function(
        userID, locales, defaultLocale, multipleRecommendedLocales) {
      $('pod-row').setPublicSessionLocales(userID,
                                           locales,
                                           defaultLocale,
                                           multipleRecommendedLocales);
    },

    /**
     * Updates the list of available keyboard layouts for a public session pod.
     * @param {string} userID The user ID of the public session
     * @param {string} locale The locale to which this list of keyboard layouts
     *     applies
     * @param {!Object} list List of available keyboard layouts
     */
    setPublicSessionKeyboardLayouts: function(userID, locale, list) {
      $('pod-row').setPublicSessionKeyboardLayouts(userID, locale, list);
    },
  };
});
