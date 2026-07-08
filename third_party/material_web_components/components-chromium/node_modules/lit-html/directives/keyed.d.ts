/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */
import { Directive, ChildPart, DirectiveParameters, DirectiveResult } from '../directive.js';
declare class Keyed<T> extends Directive {
    key: unknown;
    render(k: unknown, v: T): T;
    update(part: ChildPart, [k, v]: DirectiveParameters<this>): T;
}
interface KeyedFunc {
    <V>(k: unknown, v: V): DirectiveResult<typeof Keyed<V>>;
}
/**
 * Associates a renderable value with a unique key. When the key changes, the
 * previous DOM is removed and disposed before rendering the next value, even
 * if the value - such as a template - is the same.
 *
 * This is useful for forcing re-renders of stateful components, or working
 * with code that expects new data to generate new HTML elements, such as some
 * animation techniques.
 */
export declare const keyed: KeyedFunc;
/**
 * The type of the class that powers this directive. Necessary for naming the
 * directive's return type.
 */
export type { Keyed };
//# sourceMappingURL=keyed.d.ts.map