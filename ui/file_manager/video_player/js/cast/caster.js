// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This hack prevents a bug on the cast extension.
// TODO(yoshiki): Remove this once the cast extension supports Chrome apps.
// Although localStorage in Chrome app is not supported, but it's used in the
// cast extension. This line prevents an exception on using localStorage.
Object.defineProperty(window, 'localStorage', {
  get: function() { return {}; }
});

/**
 * @type {string}
 * @const
 */
var APPLICATION_ID = '4CCB98DA';

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', initialize);
} else {
  initialize();
}

/**
 * Starts initialization of cast-related feature.
 */
function initialize() {
  if (window.loadMockCastExtensionForTest) {
    // If the test flag is set, the mock extension for test will be laoded by
    // the test script. Sets the handler to wait for loading.
    onLoadCastSDK(initializeApi);
    return;
  }

  CastExtensionDiscoverer.findInstalledExtension(function(foundId) {
    if (foundId) {
      loadCastSDK(foundId, initializeApi);
    } else {
      console.info('No Google Cast extension is installed.');
    }
  }.wrap());
}

/**
 * Loads the Google Cast Sender SDK from the given cast extension.
 * The given callback is executes after the cast SDK is loaded.
 *
 * @param {string} extensionId ID of the extension to be loaded.
 * @param {function()} callback Callback (executed asynchronously).
 */
function loadCastSDK(extensionId, callback) {
  var script = document.createElement('script');

  var onError = function() {
    script.removeEventListener('error', onError);
    document.body.removeChild(script);
    console.error('Google Cast extension load failed.');
  }.wrap();

  // Load the Cast Sender SDK provided by the given Cast extension.
  // Legacy Cast extension relies on the extension ID being set by bootstrap
  // code, so set the ID here.
  window.chrome['cast'] = window.chrome['cast'] || {};
  window.chrome['cast']['extensionId'] = extensionId;
  script.src = 'chrome-extension://' + extensionId + '/cast_sender.js';
  script.addEventListener('error', onError);
  script.addEventListener('load', onLoadCastSDK.bind(null, callback));
  document.body.appendChild(script);
}

/**
 * Handles load event of Cast SDK and make sure the Cast API is available.
 * @param {function()} callback Callback which is called when the Caset Sender
 *     API is ready for use.
 */
function onLoadCastSDK(callback) {
  var executeCallback = function() {
    setTimeout(callback, 0);  // Runs asynchronously.
  };

  if(!chrome.cast || !chrome.cast.isAvailable) {
    var checkTimer = setTimeout(function() {
      console.error('Either "Google Cast API" or "Google Cast" extension ' +
                    'seems not to be installed?');
    }.wrap(), 5000);

    window['__onGCastApiAvailable'] = function(loaded, errorInfo) {
      clearTimeout(checkTimer);

      if (loaded) {
        executeCallback();
      } else {
        console.error('Google Cast extension load failed.', errorInfo);
      }
    }.wrap();
  } else {
    // Just executes the callback since the API is already loaded.
    executeCallback();
  }
}

/**
 * Initialize Cast API.
 */
function initializeApi() {
  var onSession = function() {
    // TODO(yoshiki): Implement this.
  };

  var onInitSuccess = function() {
    // TODO(yoshiki): Implement this.
  };

  /**
   * @param {chrome.cast.Error} error
   */
  var onError = function(error) {
    console.error('Error on Cast initialization.', error);
  };

  var sessionRequest = new chrome.cast.SessionRequest(APPLICATION_ID);
  var apiConfig = new chrome.cast.ApiConfig(sessionRequest,
                                            onSession,
                                            onReceiver);
  chrome.cast.initialize(apiConfig, onInitSuccess, onError);
}

/**
 * Called when receiver availability is changed. This method is also called when
 * initialization is completed.
 *
 * @param {chrome.cast.ReceiverAvailability} availability Availability of casts.
 * @param {Array<Object>} receivers List of casts.
 */
function onReceiver(availability, receivers) {
  if (chrome.cast.usingPresentationApi) {
    player.setCastAvailability(
        availability === chrome.cast.ReceiverAvailability.AVAILABLE);
    return;
  }

  if (availability === chrome.cast.ReceiverAvailability.AVAILABLE) {
    if (!receivers) {
      console.error('Receiver list is empty.');
      receivers = [];
    }

    metrics.recordNumberOfCastDevices(receivers.length);
    player.setCastList(receivers);
  } else if (availability == chrome.cast.ReceiverAvailability.UNAVAILABLE) {
    metrics.recordNumberOfCastDevices(0);
    player.setCastList([]);
  } else {
    console.error('Unexpected response in onReceiver.', arguments);
    player.setCastList([]);
  }
}
