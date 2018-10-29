// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('multidevice_setup', function() {
  /** @interface */
  class MojoInterfaceProvider {
    /**
     * @return {!chromeos.multideviceSetup.mojom.MultiDeviceSetupImpl}
     */
    getInterfacePtr() {}
  }

  /** @implements {multidevice_setup.MojoInterfaceProvider} */
  class MojoInterfaceProviderImpl {
    constructor() {
      /** @private {?chromeos.multideviceSetup.mojom.MultiDeviceSetupPtr} */
      this.ptr_ = null;
    }

    /** @override */
    getInterfacePtr() {
      if (!this.ptr_) {
        this.ptr_ = new chromeos.multideviceSetup.mojom.MultiDeviceSetupPtr();
        Mojo.bindInterface(
            chromeos.multideviceSetup.mojom.MultiDeviceSetup.name,
            mojo.makeRequest(this.ptr_).handle);
      }

      return this.ptr_;
    }
  }

  cr.addSingletonGetter(MojoInterfaceProviderImpl);

  return {
    MojoInterfaceProvider: MojoInterfaceProvider,
    MojoInterfaceProviderImpl: MojoInterfaceProviderImpl,
  };
});
