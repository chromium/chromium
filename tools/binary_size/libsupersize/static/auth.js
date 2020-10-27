// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @ts-check
'use strict';

let _googleAuthPromiseResolve = null;
window.googleAuth = null;
window.googleAuthPromise = new Promise((resolve, reject) => {
  _googleAuthPromiseResolve = resolve;
});

function handleClientLoad() {
  if (requiresAuthentication()) {
    gapi.load('client:auth2', initClient);
  }
}

function initClient() {
  return gapi.client.init({
      'clientId': AUTH_CLIENT_ID,
      'discoveryDocs': [AUTH_DISCOVERY_URL],
      'scope': AUTH_SCOPE,
  }).then(function () {
    window.googleAuth = gapi.auth2.getAuthInstance();
    if (!window.googleAuth.isSignedIn.get()) {
      window.googleAuth.signIn().then(setSigninStatus);
    } else {
      setSigninStatus();
    }
  });
}

function setSigninStatus() {
  let user = window.googleAuth.currentUser.get();
  _googleAuthPromiseResolve(user.getAuthResponse());
}

function requiresAuthentication() {
  // Assume everything requires auth except public trybot and one-offs.
  const queryString = decodeURIComponent(location.search);
  const isPublicTrybot = queryString.indexOf(
      'chromium-binary-size-trybot-results/android-binary-size') != -1;
  const isOneOff = queryString.indexOf('/oneoffs/') != -1;
  return !isPublicTrybot && !isOneOff;
}
