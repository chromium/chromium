/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * The base class for a mixin with an optional expected base class type.
 *
 * @template ExpectedBase Optional expected base class type, such as
 *     `LitElement`.
 *
 * @example
 * ```ts
 * interface Foo {
 *   isFoo: boolean;
 * }
 *
 * function mixinFoo<T extends MixinBase>(base: T): MixinReturn<T, Foo> {
 *   // Mixins must be `abstract`
 *   abstract class FooImpl extends base implements Foo {
 *     isFoo = true;
 *   }
 *
 *   return FooImpl;
 * }
 * ```
 */
export type MixinBase<ExpectedBase = object> = abstract new (...args: any[]) => ExpectedBase;
/**
 * The return value of a mixin.
 *
 * @template MixinBase The generic that extends `MixinBase` used for the mixin's
 *     base class argument.
 * @template MixinClass Optional interface of fuctionality that was mixed in.
 *     Omit if no additional APIs were added (such as purely overriding base
 *     class functionality).
 *
 * @example
 * ```ts
 * interface Foo {
 *   isFoo: boolean;
 * }
 *
 * // Mixins must be `abstract`
 * function mixinFoo<T extends MixinBase>(base: T): MixinReturn<T, Foo> {
 *   abstract class FooImpl extends base implements Foo {
 *     isFoo = true;
 *   }
 *
 *   return FooImpl;
 * }
 * ```
 */
export type MixinReturn<MixinBase, MixinClass = object> = (abstract new (...args: any[]) => MixinClass) & MixinBase;
