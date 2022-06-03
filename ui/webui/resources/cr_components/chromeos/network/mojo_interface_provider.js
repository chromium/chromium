// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
// #import 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-lite.js';
// #import 'chrome://resources/mojo/services/network/public/mojom/ip_address.mojom-lite.js';
// #import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
// #import 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-lite.js';
// clang-format on

// #import {addSingletonGetter} from '../../../js/cr.m.js';

cr.define('network_config', function() {
  /** @interface */
  /* #export */ class MojoInterfaceProvider {
    /** @return {!chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
    getMojoServiceRemote() {}
  }

  /** @implements {network_config.MojoInterfaceProvider} */
  /* #export */ class MojoInterfaceProviderImpl {
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

  // #cr_define_end
  return {
    MojoInterfaceProvider: MojoInterfaceProvider,
    MojoInterfaceProviderImpl: MojoInterfaceProviderImpl,
  };
});
