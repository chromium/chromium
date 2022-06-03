/**
 * @license
 * Copyright (c) 2017 The Polymer Project Authors. All rights reserved.
 * This code may only be used under the BSD style license found at
 * http://polymer.github.io/LICENSE.txt
 * The complete set of authors may be found at
 * http://polymer.github.io/AUTHORS.txt
 * The complete set of contributors may be found at
 * http://polymer.github.io/CONTRIBUTORS.txt
 * Code distributed by Google as part of the polymer project is also
 * subject to an additional IP rights grant found at
 * http://polymer.github.io/PATENTS.txt
 */
/**
 * @module lit-html
 */
import { Part } from './part.js';
import { NodePart } from './parts.js';
import { RenderOptions } from './render-options.js';
export interface TemplateProcessor {
    /**
     * Create parts for an attribute-position binding, given the element,
     * attribute name, and string literals.
     *
     * @param element The element containing the binding
     * @param name  The attribute name, including a possible prefix. The name may
     *   be prefixed by `.` (for a property binding), `@` (for an event binding)
     * or
     *   `?` (for a boolean attribute binding).
     * @param strings The array of literal strings that form the static part of
     *     the
     *   attribute value. There are always at least two strings,
     *   even for fully-controlled bindings with a single expression. For example,
     *   for the binding `attr="${e1}-${e2}"`, the `strings` array includes three
     *   strings (`['', '-', '']`)—the text _before_ the first expression (the
     * empty string), the text between the two expressions (`'-'`), and the text
     * after the last expression (another empty string).
     */
    handleAttributeExpressions(element: Element, name: string, strings: ReadonlyArray<string>, options: RenderOptions): ReadonlyArray<Part>;
    /**
     * Create parts for a text-position binding.
     * @param partOptions
     */
    handleTextExpression(options: RenderOptions): NodePart;
}
//# sourceMappingURL=template-processor.d.ts.map