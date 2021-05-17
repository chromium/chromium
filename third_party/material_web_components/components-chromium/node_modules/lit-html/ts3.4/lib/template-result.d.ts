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
import { TemplateProcessor } from './template-processor.js';
/**
 * The return type of `html`, which holds a Template and the values from
 * interpolated expressions.
 */
export declare class TemplateResult {
    readonly strings: TemplateStringsArray;
    readonly values: readonly unknown[];
    readonly type: string;
    readonly processor: TemplateProcessor;
    constructor(strings: TemplateStringsArray, values: readonly unknown[], type: string, processor: TemplateProcessor);
    /**
     * Returns a string of HTML used to create a `<template>` element.
     */
    getHTML(): string;
    getTemplateElement(): HTMLTemplateElement;
}
/**
 * A TemplateResult for SVG fragments.
 *
 * This class wraps HTML in an `<svg>` tag in order to parse its contents in the
 * SVG namespace, then modifies the template to remove the `<svg>` tag so that
 * clones only container the original fragment.
 */
export declare class SVGTemplateResult extends TemplateResult {
    getHTML(): string;
    getTemplateElement(): HTMLTemplateElement;
}
//# sourceMappingURL=template-result.d.ts.map
