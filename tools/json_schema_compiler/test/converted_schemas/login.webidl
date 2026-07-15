// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary SamlUserSessionProperties {
  // User's email address.
  required DOMString email;

  // User's Gaia ID.
  required DOMString gaiaId;

  // User's password.
  required DOMString password;

  // Oauth_code cookie set in the SAML handshake.
  required DOMString oauthCode;
};

// Event dispatched when an external logout is requested. The in-session
// extension listens for this event.
callback OnRequestExternalLogoutListener = undefined ();

interface OnRequestExternalLogoutEvent : ExtensionEvent {
  static undefined addListener(OnRequestExternalLogoutListener listener);
  static undefined removeListener(OnRequestExternalLogoutListener listener);
  static boolean hasListener(OnRequestExternalLogoutListener listener);
};

// Event dispatched when an external logout is completed. The login screen
// extension on the lock screen listens for this event.
callback OnExternalLogoutDoneListener = undefined ();

interface OnExternalLogoutDoneEvent : ExtensionEvent {
  static undefined addListener(OnExternalLogoutDoneListener listener);
  static undefined removeListener(OnExternalLogoutDoneListener listener);
  static boolean hasListener(OnExternalLogoutDoneListener listener);
};

// Use the <code>chrome.login</code> API to launch and exit user sessions.
[platforms=("chromeos"),
 implemented_in="chrome/browser/chromeos/extensions/login_screen/login/login_api.h"]
interface Login {
  // Launches a managed guest session if one is set up via the admin console.
  // If there are several managed guest sessions set up, it will launch the
  // first available one.
  // |password|: If provided, the launched managed guest session will be
  // lockable, and can only be unlocked by calling
  // $(ref:unlockManagedGuestSession) with the same password.
  // |Returns|: Note: If the function succeeds, the callback is not
  // guaranteed to be invoked as the extension will be disabled when the
  // session starts. Use this callback only to handle the failure case by
  // checking $(ref:runtime.lastError).
  static Promise<undefined> launchManagedGuestSession(
      optional DOMString password);

  // Exits the current session.
  // |dataForNextLoginAttempt|: If set, stores data which can be read by
  // $(ref:fetchDataForNextLoginAttempt) from the login screen. If unset, any
  // currently stored data will be cleared.
  static Promise<undefined> exitCurrentSession(
      optional DOMString dataForNextLoginAttempt);

  // Reads the $(ref:dataForNextLoginAttempt) set by
  // $(ref:exitCurrentSession). Clears the previously stored data after
  // reading so it can only be read once.
  // |Returns|: Called with the stored data, or an empty string if there was
  // no previously stored data.
  // |PromiseValue|: result
  static Promise<DOMString> fetchDataForNextLoginAttempt();

  // Deprecated. Please use $(ref:lockCurrentSession) instead.
  [deprecated="Use $(ref:lockCurrentSession) instead."]
  static Promise<undefined> lockManagedGuestSession();

  // Locks the current session. The session has to be either a user session or
  // a Managed Guest Session launched by $(ref:launchManagedGuestSession) with
  // a password.
  static Promise<undefined> lockCurrentSession();

  // Deprecated. Please use $(ref:unlockCurrentSession) instead.
  [deprecated="Use $(ref:unlockCurrentSession) instead."]
  static Promise<undefined> unlockManagedGuestSession(DOMString password);

  // Unlocks the current session. The session has to be either a user session
  // or a Managed Guest Session launched by $(ref:launchManagedGuestSession)
  // with a password. The session will unlock if the provided password matches
  // the one used to launch the session.
  // |password|: The password which will be used to unlock the session.
  // |Returns|: Note: If the function succeeds, the callback is not
  // guaranteed to be invoked as the extension will be disabled when the
  // session starts. Use this callback only to handle the failure case by
  // checking $(ref:runtime.lastError).
  static Promise<undefined> unlockCurrentSession(DOMString password);

  // Launches a SAML-backed user session.
  // |properties|: User's email address, gaia ID, password and oauth_code.
  // |Returns|: Note: If the function succeeds, the callback is not
  // guaranteed to be invoked as the extension will be disabled when the
  // session starts. Use this callback only to handle the failure case by
  // checking $(ref:runtime.lastError).
  static Promise<undefined> launchSamlUserSession(
      SamlUserSessionProperties properties);

  // Starts a ChromeOS Managed Guest Session which will host the shared user
  // sessions. An initial shared session is entered with |password| as the
  // password. When this shared session is locked, it can only be unlocked by
  // the same extension calling $(ref:unlockSharedSession) with the same
  // password.
  // Fails when another shared ChromeOS Managed Guest Session has already
  // been launched. Can only be called from the login screen.
  // |password|: The password which can be used to unlock the shared session.
  // |Returns|: Note: If the function succeeds, the callback is not
  // guaranteed to be invoked as the extension will be disabled when the
  // session starts. Use this callback only to handle the failure case by
  // checking $(ref:runtime.lastError).
  static Promise<undefined> launchSharedManagedGuestSession(DOMString password);

  // Enters the shared session with the given password. If the session is
  // locked, it can only be unlocked by calling $(ref:unlockSharedSession)
  // with the same password.
  // Fails if calling extension is not the same as the one which called
  // $(ref:launchSharedManagedGuestSession) or there is already a shared
  // session running. Can only be called from the lock screen.
  // |password|: The password which can be used to unlock the shared session.
  // |Returns|: Note: If the function succeeds, the callback is not
  // guaranteed to be invoked as the extension will be disabled when the
  // session starts. Use this callback only to handle the failure case by
  // checking $(ref:runtime.lastError).
  static Promise<undefined> enterSharedSession(DOMString password);

  // Unlocks the shared session with the provided password. Fails if the
  // password does not match the one provided to
  // $(ref:launchSharedManagedGuestSession) or $(ref:enterSharedSession).
  // Fails if the calling extension is not the same as the one which called
  // $(ref:launchSharedManagedGuestSession) or if there is no existing shared
  // session. Can only be called from the lock screen.
  // |password|: The password used to unlock the shared session.
  // |Returns|: Note: If the function succeeds, the callback is not
  // guaranteed to be invoked as the extension will be disabled when the
  // session starts. Use this callback only to handle the failure case by
  // checking $(ref:runtime.lastError).
  static Promise<undefined> unlockSharedSession(DOMString password);

  // Ends the shared session. Security- and privacy-sensitive data in the
  // session will be cleaned up on a best effort basis.
  // The calling extension does not have to be the same one which called
  // $(ref:launchSharedManagedGuestSession). Can be called from both the
  // lock screen or in session.
  // Fails if there is no existing shared session.
  // |Returns|: Invoked after cleanup operations have finished and the
  // session is locked.
  static Promise<undefined> endSharedSession();

  // Sets data for the next login attempt. The data can be retrieved by
  // calling $(ref:fetchDataForNextLoginAttempt). The data is cleared when
  // it is fetched so it can only be read once.
  // |dataForNextLoginAttempt|: The data to be set.
  static Promise<undefined> setDataForNextLoginAttempt(
      DOMString dataForNextLoginAttempt);

  // Dispatches a $(ref:onRequestExternalLogout) event. Called from the login
  // screen extension on the lock screen.
  static Promise<undefined> requestExternalLogout();

  // Dispatches a $(ref:onExternalLogoutDone) event. Called from the
  // in-session extension.
  static Promise<undefined> notifyExternalLogoutDone();

  // Event dispatched when an external logout is requested. The in-session
  // extension listens for this event.
  static attribute OnRequestExternalLogoutEvent onRequestExternalLogout;

  // Event dispatched when an external logout is completed. The login screen
  // extension on the lock screen listens for this event.
  static attribute OnExternalLogoutDoneEvent onExternalLogoutDone;
};

partial interface Browser {
  static attribute Login login;
};
