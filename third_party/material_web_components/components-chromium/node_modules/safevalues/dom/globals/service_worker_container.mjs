/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { unwrapResourceUrl } from '../../internals/resource_url_impl';
/** Safely registers a service worker by URL */
export function register(container, scriptURL, options) {
    return container.register(unwrapResourceUrl(scriptURL), options);
}
