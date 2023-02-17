/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * Legacy stateful foundation class for components.
 */
export declare class Foundation<Adapter extends object> {
    protected adapter: Adapter;
    constructor(adapter: Adapter);
    protected init(): void;
}
/**
 * The constructor for a foundation.
 */
export interface FoundationConstructor<Adapter extends object> {
    new (adapter: Adapter): Foundation<Adapter>;
    readonly prototype: Foundation<Adapter>;
}
/**
 * Retrieves the adapter type from the provided foundation type.
 */
export declare type AdapterOf<FoundationType> = FoundationType extends Foundation<infer A> ? A : never;
