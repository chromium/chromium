// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('multidevice_setup', function() {
  /** @interface */
  class MojoInterfaceProvider {
    /**
     * @return {!chromeos.multideviceSetup.mojom.MultiDeviceSetupRemote}
     */
    getMojoServiceRemote() {}
  }

  /** @implements {multidevice_setup.MojoInterfaceProvider} */
  class MojoInterfaceProviderImpl {
    constructor() {
      /** @private {?chromeos.multideviceSetup.mojom.MultiDeviceSetupRemote} */
      this.remote_ = null;
    }

    /** @override */
    getMojoServiceRemote() {
      if (!this.remote_) {
        this.remote_ =
            chromeos.multideviceSetup.mojom.MultiDeviceSetup.getRemote();
      }

      return this.remote_;
    }
  }

  cr.addSingletonGetter(MojoInterfaceProviderImpl);

  return {
    MojoInterfaceProvider: MojoInterfaceProvider,
    MojoInterfaceProviderImpl: MojoInterfaceProviderImpl,
  };
});
