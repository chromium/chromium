/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement } from 'lit';
import { MixinBase, MixinReturn } from './mixin.js';
/**
 * An element that can enable and disable `tabindex` focusability.
 */
export interface Focusable {
    /**
     * Whether or not the element can be focused. Defaults to true. Set to false
     * to disable focusing (unless a user has set a `tabindex`).
     */
    [isFocusable]: boolean;
}
/**
 * A property symbol that indicates whether or not a `Focusable` element can be
 * focused.
 */
export declare const isFocusable: unique symbol;
/**
 * Mixes in focusable functionality for a class.
 *
 * Elements can enable and disable their focusability with the `isFocusable`
 * symbol property. Changing `tabIndex` will trigger a lit render, meaning
 * `this.tabIndex` can be used in template expressions.
 *
 * This mixin will preserve externally-set tabindices. If an element turns off
 * focusability, but a user sets `tabindex="0"`, it will still be focusable.
 *
 * To remove user overrides and restore focusability control to the element,
 * remove the `tabindex` attribute.
 *
 * @param base The class to mix functionality into.
 * @return The provided class with `Focusable` mixed in.
 */
export declare function mixinFocusable<T extends MixinBase<LitElement>>(base: T): MixinReturn<T, Focusable>;
