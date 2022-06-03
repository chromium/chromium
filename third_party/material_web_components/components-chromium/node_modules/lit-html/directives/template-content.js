/**
 * @license
 * Copyright (c) 2020 The Polymer Project Authors. All rights reserved.
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
import { directive, NodePart } from '../lit-html.js';
// For each part, remember the value that was last rendered to the part by the
// templateContent directive, and the DocumentFragment that was last set as a
// value. The DocumentFragment is used as a unique key to check if the last
// value rendered to the part was with templateContent. If not, we'll always
// re-render the value passed to templateContent.
const previousValues = new WeakMap();
/**
 * Renders the content of a template element as HTML.
 *
 * Note, the template should be developer controlled and not user controlled.
 * Rendering a user-controlled template with this directive
 * could lead to cross-site-scripting vulnerabilities.
 */
export const templateContent = directive((template) => (part) => {
    if (!(part instanceof NodePart)) {
        throw new Error('templateContent can only be used in text bindings');
    }
    const previousValue = previousValues.get(part);
    if (previousValue !== undefined && template === previousValue.template &&
        part.value === previousValue.fragment) {
        return;
    }
    const fragment = document.importNode(template.content, true);
    part.setValue(fragment);
    previousValues.set(part, { template, fragment });
});
//# sourceMappingURL=template-content.js.map