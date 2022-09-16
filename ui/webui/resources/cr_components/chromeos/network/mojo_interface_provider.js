// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-lite.js';
import 'chrome://resources/mojo/services/network/public/mojom/ip_address.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
import 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-lite.js';

import {addSingletonGetter} from '../../../js/cr.m.js';

/** @interface */
export class MojoInterfaceProvider {
  /** @return {!chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  getMojoServiceRemote() {}
}

/** @implements {MojoInterfaceProvider} */
export class MojoInterfaceProviderImpl {
  constructor() {
    /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
    this.remote_ = null;
  }

  /** @override */
  getMojoServiceRemote() {
    if (!this.remote_) {
      this.remote_ = chromeos.networkConfig.mojom.CrosNetworkConfig.getRemote();
    }

    return this.remote_;
  }
}

addSingletonGetter(MojoInterfaceProviderImpl);
