/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
/// <reference types="trusted-types" />
/**
 * Returns window.trustedTypes if Trusted Types are enabled and supported, or
 * null otherwise.
 */
export declare function getTrustedTypes(): TrustedTypePolicyFactory | null;
/**
 * Returns the Trusted Types policy used by TS safevalues, or null if Trusted
 * Types are not enabled/supported. The first call to this function will
 * create the policy.
 */
export declare function getTrustedTypesPolicy(): TrustedTypePolicy | null;
/** Helpers for tests. */
export declare const TEST_ONLY: {
    resetDefaults(): void;
    setTrustedTypesPolicyName(name: string): void;
};
