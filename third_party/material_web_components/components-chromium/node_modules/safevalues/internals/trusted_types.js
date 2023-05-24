"use strict";
/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.TEST_ONLY = exports.getTrustedTypesPolicy = exports.getTrustedTypes = void 0;
/**
 * The name of the Trusted Types policy used by TS safevalues, or empty
 * to disable Trusted Types. This duplicates the 'google#safe', but
 * can be overridden in tests.
 */
var trustedTypesPolicyName = 'google#safe';
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
function getTrustedTypes() {
    var _a;
    return (trustedTypesPolicyName !== '') ? ((_a = trustedTypes()) !== null && _a !== void 0 ? _a : null) : null;
}
exports.getTrustedTypes = getTrustedTypes;
/**
 * The Trusted Types policy used by TS safevalues, or null if Trusted Types
 * are not enabled/supported, or undefined if the policy has not been created
 * yet.
 */
var trustedTypesPolicy;
/**
 * Returns the Trusted Types policy used by TS safevalues, or null if Trusted
 * Types are not enabled/supported. The first call to this function will
 * create the policy.
 */
function getTrustedTypesPolicy() {
    var _a, _b;
    if (trustedTypesPolicy === undefined) {
        try {
            trustedTypesPolicy =
                (_b = (_a = getTrustedTypes()) === null || _a === void 0 ? void 0 : _a.createPolicy(trustedTypesPolicyName, {
                    createHTML: function (s) { return s; },
                    createScript: function (s) { return s; },
                    createScriptURL: function (s) { return s; }
                })) !== null && _b !== void 0 ? _b : null;
        }
        catch (_c) {
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
exports.getTrustedTypesPolicy = getTrustedTypesPolicy;
/** Helpers for tests. */
exports.TEST_ONLY = {
    resetDefaults: function () {
        trustedTypesPolicy = undefined;
        trustedTypesPolicyName = 'google#safe';
    },
    setTrustedTypesPolicyName: function (name) {
        trustedTypesPolicyName = name;
    },
};
