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
import { Template } from './template.js';
/**
 * A function type that creates a Template from a TemplateResult.
 *
 * This is a hook into the template-creation process for rendering that
 * requires some modification of templates before they're used, like ShadyCSS,
 * which must add classes to elements and remove styles.
 *
 * Templates should be cached as aggressively as possible, so that many
 * TemplateResults produced from the same expression only do the work of
 * creating the Template the first time.
 *
 * Templates are usually cached by TemplateResult.strings and
 * TemplateResult.type, but may be cached by other keys if this function
 * modifies the template.
 *
 * Note that currently TemplateFactories must not add, remove, or reorder
 * expressions, because there is no way to describe such a modification
 * to render() so that values are interpolated to the correct place in the
 * template instances.
 */
export declare type TemplateFactory = (result: TemplateResult) => Template;
/**
 * The default TemplateFactory which caches Templates keyed on
 * result.type and result.strings.
 */
export declare function templateFactory(result: TemplateResult): Template;
/**
 * The first argument to JS template tags retain identity across multiple
 * calls to a tag for the same literal, so we can cache work done per literal
 * in a Map.
 *
 * Safari currently has a bug which occasionally breaks this behavior, so we
 * need to cache the Template at two levels. We first cache the
 * TemplateStringsArray, and if that fails, we cache a key constructed by
 * joining the strings array.
 */
export interface TemplateCache {
    readonly stringsArray: WeakMap<TemplateStringsArray, Template>;
    readonly keyString: Map<string, Template>;
}
export declare const templateCaches: Map<string, TemplateCache>;
//# sourceMappingURL=template-factory.d.ts.map
