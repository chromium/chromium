// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/resources/test-only-api.js';
import { EnvironmentIntegrityService, EnvironmentIntegrityServiceReceiver } from '/gen/third_party/blink/public/mojom/environment_integrity/environment_integrity_service.mojom.m.js';

export class MockEnvironmentIntegrity {
    #interceptor;
    #receiver;

    // TODO(crbug.com/1439945): Temporary variable until the results from getEnvironmentIntegrity
    // are actually used for something.
    #wasCalled;

    constructor() {
        this.#receiver = new EnvironmentIntegrityServiceReceiver(this);

        this.#interceptor =
            new MojoInterfaceInterceptor(EnvironmentIntegrityService.$interfaceName);

        this.#interceptor.oninterfacerequest =
            e => this.#receiver.$.bindHandle(e.handle);
        this.#interceptor.start();

        this.#wasCalled = false;
    }

    async getEnvironmentIntegrity() {
        this.#wasCalled = true;
        return 0;
    }

    get wasCalled() {
        return this.#wasCalled;
    }

    stop() {
        this.#receiver.$.close();
        this.#interceptor.stop();
    }
}
