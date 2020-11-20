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

function toggleSigninModal(show) {
  const modal = document.getElementById('signin-modal');
  if (show) {
    modal.style.display = 'block';
  } else {
    modal.style.display = 'none';
  }
}

function initClient() {
  const signin_button = document.querySelector('#signin-modal button.signin')
  signin_button.addEventListener('click', () => {
    window.googleAuth.signIn().then(setSigninStatus);
    toggleSigninModal(false /*show*/);
  });
  return gapi.client.init({
      'clientId': AUTH_CLIENT_ID,
      'discoveryDocs': [AUTH_DISCOVERY_URL],
      'scope': AUTH_SCOPE,
  }).then(function () {
    window.googleAuth = gapi.auth2.getAuthInstance();
    if (!window.googleAuth.isSignedIn.get() && requiresAuthentication()) {
      toggleSigninModal(true /*show*/);
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
