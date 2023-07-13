// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Retrieve a WEI verdict from another origin. This is useful for testing the
 * partitioning across origins.
 *
 * @param {string} origin Origin you want to get an WEI verdict from
 * @param {string} contentBinding The content binding you'd like to use
 *
 * @returns Base64 encoded token from other origin
 */
export async function getTokenFromOtherOrigin(origin, contentBinding) {
    const otherWindow = window.open(`${origin}/wpt_internal/environment-integrity/resources/otherOrigin.sub.https.html`);

    let complete = null;
    let result = new Promise(res => complete = res);

    window.addEventListener('message', (e) => {
        const response = JSON.parse(e.data);
        switch (response.type) {
            case 'ready': {
                otherWindow.postMessage(contentBinding, '*');
                break;
            };
            case 'token': {
                otherWindow.close();
                complete(response.data);
                break;
            }
        }
    });

    return result;
}
