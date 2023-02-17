/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Retrieve all keys from type `T` and extract those whose value types are
 * assignable to `V`.
 *
 * @template T The type to retrieve keys from.
 * @template V The extracted keys' value type.
 */
export type ExtractKeysOfType<T extends object, V> = {
  [K in keyof T]: T[K] extends V ? K : never;
}[keyof T];

/**
 * Retrieve all keys from type `T` and exclude those whose value types are
 * assignable to `V`.
 *
 * @template T The type to retrieve keys from.
 * @template V The excluded keys' value type.
 */
export type ExcludeKeysOfType<T extends object, V> = {
  [K in keyof T]: T[K] extends V ? never : K;
}[keyof T];

/**
 * Retrieves all keys from type `T` whose value types are function.
 *
 * @template T The type to retrieve keys from.
 * @example
 * class Foo {
 *   property = false;
 *   method() {}
 * }
 *
 * type AllKeys = keyof Foo; // 'property'|'method'
 * type FooFunctionKeys = FunctionKeys<Foo>; // 'method'
 */
export type FunctionKeys<T extends object> = ExtractKeysOfType<T, Function>;

/**
 * Retrieves all keys from type `T` whose value types are not functions.
 *
 * @template T The type to retrieve keys from.
 * @example
 * interface Foo {
 *   property = false;
 *   method() {}
 * }
 *
 * type AllKeys = keyof Foo; // 'property'|'method'
 * type FooFunctionKeys = PropertyKeys<Foo>; // 'property'
 */
export type PropertyKeys<T extends object> = ExcludeKeysOfType<T, Function>;
