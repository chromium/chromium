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
  const btnSignin = g_el.divSigninModal.querySelector('button.signin');
  btnSignin.addEventListener('click', getToken);

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
  g_el.divSigninModal.style.display = show ? '' : 'none';
}

/**
 * @param {!Array<!URL>} urlsToLoad
 * @return {boolean}
 */
function requiresAuthentication(urlsToLoad) {
  for (const url of urlsToLoad) {
    // Files from ocalhost, public trybot, and one-offs don't need auth.
    if (url.hostname === 'localhost')
      continue;
    if (url.pathname.startsWith(
            '/chromium-binary-size-trybot-results/android-binary-size/')) {
      continue;
    }
    if (url.pathname.indexOf('/oneoffs/') >= 0)
      continue;
    // Assume everything else requires auth.
    return true;
  }
  return false;
}
