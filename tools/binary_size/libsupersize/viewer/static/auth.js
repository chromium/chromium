// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';


/** @type {(a: TokenClient) => void} Resolve function of g_authScriptPromise. */
let g_onAuthScriptLoaded = null;
/** @type {Promise<TokenClient>} Resolves when GIS is loaded. */
const g_authScriptPromise = new Promise((resolve, reject) => {
  g_onAuthScriptLoaded = resolve;
});

/** @type {(a: string) => void} Resolve function of g_authTokenPromise. */
let g_onTokenReceived = null;
/** @type {Promise<string>} Resolves when access token is received. */
const g_authTokenPromise = new Promise((resolve, reject) => {
  g_onTokenReceived = resolve;
});

/** @type {boolean} Used to ensure the auth pipeline is only setup once. */
let g_authFetchCalled = false;

/** @return {Promise<*>} */
async function initClient() {
  let tokenClient = google.accounts.oauth2.initTokenClient({
    client_id: AUTH_CLIENT_ID,
    scope: AUTH_SCOPE,
    callback: '',  // defined at request time in await/promise scope.
  });
  g_onAuthScriptLoaded(tokenClient);
}

/** @return {Promise<void>} */
async function getToken() {
  let tokenClient = await g_authScriptPromise;
  try {
    tokenClient.callback = (resp) => {
      if (resp.error === undefined) {
        g_onTokenReceived(resp.access_token);
      }
    };
    tokenClient.requestAccessToken();
  } catch (err) {
    console.error(err)
  }
}

/** @return {Promise<void>} */
async function doAuthFetch() {
  // TODO(mheikal): cache access token in localstorage and check if expired here
  // and return cached token.

  // Show a "Please sign in" dialog.
  toggleSigninModal(true /*show*/);
  let signinButton = document.querySelector('#signin-modal button.signin');
  signinButton.addEventListener('click', getToken);

  // Only hide the dialog once we have the token.
  await g_authTokenPromise;
  toggleSigninModal(false /*show*/);
}

/** @return {Promise<?string>} */
async function fetchAccessToken() {
  if (!g_authFetchCalled) {
    doAuthFetch();
    g_authFetchCalled = true;
  }
  return await g_authTokenPromise;
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
