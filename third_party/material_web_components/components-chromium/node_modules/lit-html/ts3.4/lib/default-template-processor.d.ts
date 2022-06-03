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
import { Part } from './part.js';
import { NodePart } from './parts.js';
import { RenderOptions } from './render-options.js';
import { TemplateProcessor } from './template-processor.js';
/**
 * Creates Parts when a template is instantiated.
 */
export declare class DefaultTemplateProcessor implements TemplateProcessor {
    /**
     * Create parts for an attribute-position binding, given the event, attribute
     * name, and string literals.
     *
     * @param element The element containing the binding
     * @param name  The attribute name
     * @param strings The string literals. There are always at least two strings,
     *   event for fully-controlled bindings with a single expression.
     */
    handleAttributeExpressions(element: Element, name: string, strings: string[], options: RenderOptions): ReadonlyArray<Part>;
    /**
     * Create parts for a text-position binding.
     * @param templateFactory
     */
    handleTextExpression(options: RenderOptions): NodePart;
}
export declare const defaultTemplateProcessor: DefaultTemplateProcessor;
//# sourceMappingURL=default-template-processor.d.ts.map
