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
import { removeNodes, reparentNodes } from '../lib/dom.js';
/**
 * A lightweight <template> polyfill that supports minimum features to cover
 * lit-html use cases. It provides an alternate route in case <template> is not
 * natively supported.
 * Please note that nested template, cloning template node and innerHTML getter
 * do NOT work with this polyfill.
 * If it can not fulfill your requirement, please consider using the full
 * polyfill: https://github.com/webcomponents/template
 */
export const initTemplatePolyfill = (forced = false) => {
    // Minimal polyfills (like this one) may provide only a subset of Template's
    // functionality. So, we explicitly check that at least content is present to
    // prevent installing patching with multiple polyfills, which might happen if
    // multiple versions of lit-html were included on a page.
    if (!forced && 'content' in document.createElement('template')) {
        return;
    }
    const contentDoc = document.implementation.createHTMLDocument('template');
    const body = contentDoc.body;
    const descriptor = {
        enumerable: true,
        configurable: true,
    };
    const upgrade = (template) => {
        const content = contentDoc.createDocumentFragment();
        Object.defineProperties(template, {
            content: Object.assign(Object.assign({}, descriptor), { get() {
                    return content;
                } }),
            innerHTML: Object.assign(Object.assign({}, descriptor), { set: function (text) {
                    body.innerHTML = text;
                    removeNodes(content, content.firstChild);
                    reparentNodes(content, body.firstChild);
                } }),
        });
    };
    const capturedCreateElement = Document.prototype.createElement;
    Document.prototype.createElement = function createElement(tagName, options) {
        const el = capturedCreateElement.call(this, tagName, options);
        if (el.tagName === 'TEMPLATE') {
            upgrade(el);
        }
        return el;
    };
};
//# sourceMappingURL=template_polyfill.js.map