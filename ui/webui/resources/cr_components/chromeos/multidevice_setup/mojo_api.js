// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
// #import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
// #import 'chrome://resources/mojo/chromeos/components/multidevice/mojom/multidevice_types.mojom-lite.js';
// #import 'chrome://resources/mojo/chromeos/services/device_sync/public/mojom/device_sync.mojom-lite.js';
// #import 'chrome://resources/mojo/chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom-lite.js';
// clang-format on

cr.define('multidevice_setup', function() {
  /** @interface */
  /* #export */ class MojoInterfaceProvider {
    /**
     * @return {!chromeos.multideviceSetup.mojom.MultiDeviceSetupRemote}
     */
    getMojoServiceRemote() {}
  }

  /** @implements {multidevice_setup.MojoInterfaceProvider} */
  /* #export */ class MojoInterfaceProviderImpl {
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

  // #cr_define_end
  return {
    MojoInterfaceProvider: MojoInterfaceProvider,
    MojoInterfaceProviderImpl: MojoInterfaceProviderImpl,
  };
});
