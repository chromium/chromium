/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { unwrapResourceUrl } from '../../internals/resource_url_impl';
import { unwrapScript } from '../../internals/script_impl';
/** Returns CSP nonce, if set for any script tag. */
function getScriptNonceFromWindow(win) {
    const doc = win.document;
    // document.querySelector can be undefined in non-browser environments.
    const script = doc.querySelector?.('script[nonce]');
    if (script) {
        // Try to get the nonce from the IDL property first, because browsers that
        // implement additional nonce protection features (currently only Chrome) to
        // prevent nonce stealing via CSS do not expose the nonce via attributes.
        // See https://github.com/whatwg/html/issues/2369
        return script['nonce'] || script.getAttribute('nonce') || '';
    }
    return '';
}
/** Propagates CSP nonce to dynamically created scripts. */
function setNonceForScriptElement(script) {
    const win = script.ownerDocument && script.ownerDocument.defaultView;
    const nonce = getScriptNonceFromWindow(win || window);
    if (nonce) {
        script.setAttribute('nonce', nonce);
    }
}
/** Sets textContent from the given SafeScript. */
export function setTextContent(script, v) {
    script.textContent = unwrapScript(v);
    setNonceForScriptElement(script);
}
/** Sets the Src attribute using a TrustedResourceUrl */
export function setSrc(script, v) {
    script.src = unwrapResourceUrl(v);
    setNonceForScriptElement(script);
}
