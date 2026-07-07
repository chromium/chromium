// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Use the <code>chrome.loginScreenStorage</code> API to store persistent data
// from the login screen or inject data into the session.
[platforms=("chromeos"),
 implemented_in="chrome/browser/chromeos/extensions/login_screen/login_screen_storage/login_screen_storage_api.h"]
interface LoginScreenStorage {
  // Stores persistent data from the login screen. This data can be accessed
  // later using $(ref:retrievePersistentData) by any extension from the
  // specified extension ids. This method will fail if called while a user
  // session is active.
  // |extensionIds|: IDs of the extensions that should have access to the
  // stored data.
  // |data|: The data to store.
  static Promise<undefined> storePersistentData(
      sequence<DOMString> extensionIds,
      DOMString data);

  // Retrieves persistent data that was previously stored using
  // $(ref:storePersistentData) for the caller's extension ID.
  // |ownerId|: ID of the extension that saved the data that the caller is
  // trying to retrieve.
  // |PromiseValue|: data
  static Promise<DOMString> retrievePersistentData(DOMString ownerId);

  // Stores credentials for later access from the user session. This method
  // will fail if called while a user session is active.
  // |extensionId|: ID of the in-session extension that should have access to
  // these credentials. Credentials stored using this method are deleted on
  // session exit.
  // |credentials|: The credentials to store.
  static Promise<undefined> storeCredentials(
      DOMString extensionId,
      DOMString credentials);

  // Retrieves credentials that were previosly stored using
  // $(ref:storeCredentials). The caller's extension ID should be the same as
  // the extension id passed to the $(ref:storeCredentials).
  // |PromiseValue|: data
  static Promise<DOMString> retrieveCredentials();
};

partial interface Browser {
  static attribute LoginScreenStorage loginScreenStorage;
};
