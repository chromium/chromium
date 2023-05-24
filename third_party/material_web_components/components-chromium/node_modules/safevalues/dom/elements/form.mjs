/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { unwrapUrlOrSanitize } from '../../builders/url_sanitizer';
/**
 * Sets the Action attribute from the given Url.
 */
export function setAction(form, url) {
    const sanitizedUrl = unwrapUrlOrSanitize(url);
    if (sanitizedUrl !== undefined) {
        form.action = sanitizedUrl;
    }
}
