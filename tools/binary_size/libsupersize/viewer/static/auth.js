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

/** localStorage key for the most recent access token and its expiry. */
const AUTH_TOKEN_STORAGE_KEY = 'authToken';
/** Treat tokens as expired this long before they actually expire. */
const AUTH_TOKEN_EXPIRY_MARGIN_MS = 60 * 1000;

/** @return {?string} A previously granted access token that is still valid. */
function loadCachedToken() {
  try {
    const cached = JSON.parse(localStorage.getItem(AUTH_TOKEN_STORAGE_KEY));
    if (cached && cached.expiresAt - AUTH_TOKEN_EXPIRY_MARGIN_MS > Date.now()) {
      return cached.token;
    }
  } catch (err) {
    console.error(err);
  }
  return null;
}

/**
 * Forgets the cached token, e.g. after it was rejected because it was revoked
 * or belongs to a different account.
 */
function clearCachedToken() {
  localStorage.removeItem(AUTH_TOKEN_STORAGE_KEY);
}

/** @param {{access_token: string, expires_in: (number|string)}} resp */
function saveToken(resp) {
  const expiresAt = Date.now() + Number(resp.expires_in) * 1000;
  localStorage.setItem(
      AUTH_TOKEN_STORAGE_KEY,
      JSON.stringify({token: resp.access_token, expiresAt}));
}

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
        saveToken(resp);
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
  // Reuse a token from an earlier session if it has not expired.
  const cachedToken = loadCachedToken();
  if (cachedToken) {
    g_onTokenReceived(cachedToken);
    return;
  }

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
