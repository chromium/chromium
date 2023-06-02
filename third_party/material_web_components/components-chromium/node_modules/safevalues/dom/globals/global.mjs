/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { unwrapScript } from '../../internals/script_impl';
/**
 * Evaluates a SafeScript value in the given scope using eval.
 *
 * Strongly consider avoiding this, as eval blocks CSP adoption and does not
 * benefit from compiler optimizations.
 */
export function globalEval(win, script) {
    const trustedScript = unwrapScript(script);
    let result = win.eval(trustedScript);
    if (result === trustedScript) {
        // https://crbug.com/1024786 manifesting in workers.
        result = win.eval(trustedScript.toString());
    }
    return result;
}
