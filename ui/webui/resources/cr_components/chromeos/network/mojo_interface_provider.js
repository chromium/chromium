// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('network_config', function() {
  /** @interface */
  class MojoInterfaceProvider {
    /** @return {!chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
    getMojoServiceRemote() {}
  }

  /** @implements {network_config.MojoInterfaceProvider} */
  class MojoInterfaceProviderImpl {
    constructor() {
      /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
      this.remote_ = null;
    }

    /** @override */
    getMojoServiceRemote() {
      if (!this.remote_) {
        this.remote_ =
            chromeos.networkConfig.mojom.CrosNetworkConfig.getRemote();
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
