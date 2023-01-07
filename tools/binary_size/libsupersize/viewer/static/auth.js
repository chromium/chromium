// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @type {Promise} Resolves when gapi.js is loaded. */
const g_authScriptPromise = new Promise((resolve, reject) => {
  window.onAuthScriptLoaded = resolve;
});

/** @type {?Promise} Resolves when auth has completed. */
let g_doAuthPromise = null;

/** @return {Promise<*>} */
async function initAuthApi() {
  await g_authScriptPromise;
  await new Promise((resolve, reject) => {
    gapi.load('client:auth2', resolve);
  });
  await gapi.client.init({
      'clientId': AUTH_CLIENT_ID,
      'discoveryDocs': [AUTH_DISCOVERY_URL],
      'scope': AUTH_SCOPE,
  })
  return gapi.auth2.getAuthInstance();
}

/** @return {Promise<*>} */
async function doAuthFetch() {
  let googleAuth = await initAuthApi();

  // See if already signed in.
  if (googleAuth.isSignedIn.get()) {
    return googleAuth.currentUser.get();
  }

  // Show a "Please sign in" dialog.
  toggleSigninModal(true /*show*/);

  // Loop in case the pop-up dialog is closed without signing in.
  while (true) {
    await new Promise((resolve, reject) => {
      let signinButton = document.querySelector('#signin-modal button.signin');
      signinButton.addEventListener('click', resolve, {once: true});
    });

    try {
      await googleAuth.signIn();
      break;
    } catch {
      // Allow them to click dialog button again.
    }
  }
  toggleSigninModal(false /*show*/);

  return googleAuth.currentUser.get();
}

/** @return {Promise<?string>} */
async function fetchAccessToken() {
  if (!g_doAuthPromise) {
    g_doAuthPromise = doAuthFetch();
  }
  let user = await g_doAuthPromise;
  return user.getAuthResponse().access_token;
}

/** @param {boolean} show */
function toggleSigninModal(show) {
  const modal = document.getElementById('signin-modal');
  modal.style.display = show ? '': 'none';
}

/** @return {boolean} */
function requiresAuthentication() {
  // Assume everything requires auth except public trybot and one-offs.
  const queryString = decodeURIComponent(location.search);
  const isPublicTrybot = queryString.indexOf(
      'chromium-binary-size-trybot-results/android-binary-size') != -1;
  const isOneOff = queryString.indexOf('/oneoffs/') != -1;
  return !isPublicTrybot && !isOneOff;
}
