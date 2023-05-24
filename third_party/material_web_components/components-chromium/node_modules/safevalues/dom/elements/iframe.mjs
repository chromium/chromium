/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @fileoverview Safe iframe helpers and go/intents-for-iframes-for-closure
 */
import { unwrapHtml } from '../../internals/html_impl';
import { unwrapResourceUrl } from '../../internals/resource_url_impl';
/** Sets the Src attribute using a TrustedResourceUrl */
export function setSrc(iframe, v) {
    iframe.src = unwrapResourceUrl(v).toString();
}
/** Sets the Srcdoc attribute using a SafeHtml */
export function setSrcdoc(iframe, v) {
    iframe.srcdoc = unwrapHtml(v);
}
