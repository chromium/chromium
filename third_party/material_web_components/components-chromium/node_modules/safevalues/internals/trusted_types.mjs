/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * The name of the Trusted Types policy used by TS safevalues, or empty
 * to disable Trusted Types. This duplicates the 'google#safe', but
 * can be overridden in tests.
 */
let trustedTypesPolicyName = 'google#safe';
/** Helper to retrieve the value of `window.trustedTypes`. */
function trustedTypes() {
    if (typeof window !== 'undefined') {
        return window.trustedTypes;
    }
    return undefined;
}
/**
 * Returns window.trustedTypes if Trusted Types are enabled and supported, or
 * null otherwise.
 */
export function getTrustedTypes() {
    return (trustedTypesPolicyName !== '') ? (trustedTypes() ?? null) : null;
}
/**
 * The Trusted Types policy used by TS safevalues, or null if Trusted Types
 * are not enabled/supported, or undefined if the policy has not been created
 * yet.
 */
let trustedTypesPolicy;
/**
 * Returns the Trusted Types policy used by TS safevalues, or null if Trusted
 * Types are not enabled/supported. The first call to this function will
 * create the policy.
 */
export function getTrustedTypesPolicy() {
    if (trustedTypesPolicy === undefined) {
        try {
            trustedTypesPolicy =
                getTrustedTypes()?.createPolicy(trustedTypesPolicyName, {
                    createHTML: (s) => s,
                    createScript: (s) => s,
                    createScriptURL: (s) => s
                }) ??
                    null;
        }
        catch {
            // In Chromium versions before 81, trustedTypes.createPolicy throws if
            // called with a name that is already registered, even if no CSP is set.
            // Until users have largely migrated to 81 or above, catch the error not
            // to break the applications functionally. In such case, the code will
            // fall back to using regular Safe Types.
            trustedTypesPolicy = null;
        }
    }
    return trustedTypesPolicy;
}
/** Helpers for tests. */
export const TEST_ONLY = {
    resetDefaults() {
        trustedTypesPolicy = undefined;
        trustedTypesPolicyName = 'google#safe';
    },
    setTrustedTypesPolicyName(name) {
        trustedTypesPolicyName = name;
    },
};
