/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { unwrapResourceUrl } from '../../internals/resource_url_impl';
/**
 * Sets the Src attribute from the given SafeUrl.
 */
export function setSrc(embedEl, url) {
    embedEl.src = unwrapResourceUrl(url);
}
