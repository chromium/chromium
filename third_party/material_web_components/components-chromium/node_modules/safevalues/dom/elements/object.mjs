/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { unwrapResourceUrl } from '../../internals/resource_url_impl';
/** Sets the data attribute using a TrustedResourceUrl */
export function setData(obj, v) {
    obj.data = unwrapResourceUrl(v);
}
