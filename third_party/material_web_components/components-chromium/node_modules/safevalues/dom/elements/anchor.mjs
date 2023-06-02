/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { unwrapUrlOrSanitize } from '../../builders/url_sanitizer';
/**
 * Sets the Href attribute from the given Url.
 */
export function setHref(anchor, url) {
    const sanitizedUrl = unwrapUrlOrSanitize(url);
    if (sanitizedUrl !== undefined) {
        anchor.href = sanitizedUrl;
    }
}
