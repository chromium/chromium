/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { unwrapUrlOrSanitize } from '../../builders/url_sanitizer';
/**
 * Sets the Formaction attribute from the given Url.
 */
export function setFormaction(button, url) {
    const sanitizedUrl = unwrapUrlOrSanitize(url);
    if (sanitizedUrl !== undefined) {
        button.formAction = sanitizedUrl;
    }
}
