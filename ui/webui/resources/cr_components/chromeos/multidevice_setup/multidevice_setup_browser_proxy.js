// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('multidevice_setup', function() {
  /** @interface */
  class BrowserProxy {
    /**
     * Requests profile information; namely, a dictionary containing the user's
     * e-mail address and profile photo.
     * @return {!Promise<{profilePhotoUrl: string, email: string}>}
     */
    getProfileInfo() {}

    /**
     * Opens settings to the MultiDevice individual feature settings subpage.
     * (a.k.a. Connected Devices).
     */
    openMultiDeviceSettings() {}
  }

  /** @implements {multidevice_setup.BrowserProxy} */
  class BrowserProxyImpl {
    /** @override */
    getProfileInfo() {
      return cr.sendWithPromise('getProfileInfo');
    }

    /** @override */
    openMultiDeviceSettings() {
      chrome.send('openMultiDeviceSettings');
    }
  }

  cr.addSingletonGetter(BrowserProxyImpl);

  return {
    BrowserProxy: BrowserProxy,
    BrowserProxyImpl: BrowserProxyImpl,
  };
});
