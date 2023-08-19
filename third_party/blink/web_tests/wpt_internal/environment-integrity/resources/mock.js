// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/resources/test-only-api.js';
import {
    EnvironmentIntegrityService,
    EnvironmentIntegrityServiceReceiver,
    EnvironmentIntegrityResponseCode,
} from '/gen/third_party/blink/public/mojom/environment_integrity/environment_integrity_service.mojom.m.js';

export { EnvironmentIntegrityResponseCode } from '/gen/third_party/blink/public/mojom/environment_integrity/environment_integrity_service.mojom.m.js';

/**
 * We encode the content binding with SHA512 so this is a useful utility function
 * for the tests for quick comparisons.
 *
 * @param {string} str Whatever you want to convert
 * @returns {Promise<ArrayBuffer>}
 */
export async function convertStringToSha256(str) {
    return crypto.subtle.digest(
        "SHA-256",
        new TextEncoder().encode(str),
    );
}

/**
 * @param {ArrayBuffer} arrayBuffer
 * @returns {String} Base64 string
 */
export function convertArrayBufferToBase64(arrayBuffer) {
    return btoa(String.fromCharCode(...new Uint8Array(arrayBuffer)));
}

export class MockEnvironmentIntegrity {
    #interceptor;
    #receiver;
    #responseCode;

    constructor() {
        this.#receiver = new EnvironmentIntegrityServiceReceiver(this);

        this.#interceptor =
            new MojoInterfaceInterceptor(EnvironmentIntegrityService.$interfaceName);

        this.#interceptor.oninterfacerequest =
            e => this.#receiver.$.bindHandle(e.handle);
        this.#interceptor.start();

        this.#responseCode = EnvironmentIntegrityResponseCode.kSuccess;
    }

    /**
     * Change the mocked response code back from the attester.
     *
     * @param {number} responseCode
     */
    mockResponseCode(responseCode) {
        this.#responseCode = responseCode;
    }

    async getEnvironmentIntegrity(contentBinding) {
        // For testing purposes, it is easiest to just return the content binding
        // as the "token"
        return Promise.resolve({
            responseCode: this.#responseCode,
            token: contentBinding,
        });
    }

    stop() {
        this.#receiver.$.close();
        this.#interceptor.stop();
    }
}
