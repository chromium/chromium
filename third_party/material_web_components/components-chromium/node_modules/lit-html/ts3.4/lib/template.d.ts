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
import { TemplateResult } from './template-result.js';
/**
 * An expression marker with embedded unique key to avoid collision with
 * possible text in templates.
 */
export declare const marker: string;
/**
 * An expression marker used text-positions, multi-binding attributes, and
 * attributes with markup-like text values.
 */
export declare const nodeMarker: string;
export declare const markerRegex: RegExp;
/**
 * Suffix appended to all bound attribute names.
 */
export declare const boundAttributeSuffix = "$lit$";
/**
 * An updatable Template that tracks the location of dynamic parts.
 */
export declare class Template {
    readonly parts: TemplatePart[];
    readonly element: HTMLTemplateElement;
    constructor(result: TemplateResult, element: HTMLTemplateElement);
}
/**
 * A placeholder for a dynamic expression in an HTML template.
 *
 * There are two built-in part types: AttributePart and NodePart. NodeParts
 * always represent a single dynamic expression, while AttributeParts may
 * represent as many expressions are contained in the attribute.
 *
 * A Template's parts are mutable, so parts can be replaced or modified
 * (possibly to implement different template semantics). The contract is that
 * parts can only be replaced, not removed, added or reordered, and parts must
 * always consume the correct number of values in their `update()` method.
 *
 * TODO(justinfagnani): That requirement is a little fragile. A
 * TemplateInstance could instead be more careful about which values it gives
 * to Part.update().
 */
export declare type TemplatePart = {
    readonly type: 'node';
    index: number;
} | {
    readonly type: 'attribute';
    index: number;
    readonly name: string;
    readonly strings: ReadonlyArray<string>;
};
export declare const isTemplatePartActive: (part: TemplatePart) => boolean;
export declare const createMarker: () => Comment;
/**
 * This regex extracts the attribute name preceding an attribute-position
 * expression. It does this by matching the syntax allowed for attributes
 * against the string literal directly preceding the expression, assuming that
 * the expression is in an attribute-value position.
 *
 * See attributes in the HTML spec:
 * https://www.w3.org/TR/html5/syntax.html#elements-attributes
 *
 * " \x09\x0a\x0c\x0d" are HTML space characters:
 * https://www.w3.org/TR/html5/infrastructure.html#space-characters
 *
 * "\0-\x1F\x7F-\x9F" are Unicode control characters, which includes every
 * space character except " ".
 *
 * So an attribute is:
 *  * The name: any character except a control character, space character, ('),
 *    ("), ">", "=", or "/"
 *  * Followed by zero or more space characters
 *  * Followed by "="
 *  * Followed by zero or more space characters
 *  * Followed by:
 *    * Any character except space, ('), ("), "<", ">", "=", (`), or
 *    * (") then any non-("), or
 *    * (') then any non-(')
 */
export declare const lastAttributeNameRegex: RegExp;
//# sourceMappingURL=template.d.ts.map
