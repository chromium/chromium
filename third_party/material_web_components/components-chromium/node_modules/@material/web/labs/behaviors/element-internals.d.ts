/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement } from 'lit';
import { MixinBase, MixinReturn } from './mixin.js';
/**
 * A unique symbol used for protected access to an instance's
 * `ElementInternals`.
 *
 * @example
 * ```ts
 * class MyElement extends mixinElementInternals(LitElement) {
 *   constructor() {
 *     super();
 *     this[internals].role = 'button';
 *   }
 * }
 * ```
 */
export declare const internals: unique symbol;
/**
 * An instance with an `internals` symbol property for the component's
 * `ElementInternals`.
 *
 * Use this when protected access is needed for an instance's `ElementInternals`
 * from other files. A unique symbol is used to access the internals.
 */
export interface WithElementInternals {
    /**
     * An instance's `ElementInternals`.
     */
    [internals]: ElementInternals;
}
/**
 * Mixes in an attached `ElementInternals` instance.
 *
 * This mixin is only needed when other shared code needs access to a
 * component's `ElementInternals`, such as form-associated mixins.
 *
 * @param base The class to mix functionality into.
 * @return The provided class with `WithElementInternals` mixed in.
 */
export declare function mixinElementInternals<T extends MixinBase<LitElement>>(base: T): MixinReturn<T, WithElementInternals>;
