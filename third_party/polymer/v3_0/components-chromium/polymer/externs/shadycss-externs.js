/** @externs */

/** @typedef {{
 * styleElement: function(!HTMLElement),
 * styleSubtree: function(!HTMLElement, Object<string, string>=),
 * prepareTemplate: function(!HTMLTemplateElement, string, string=),
 * prepareTemplateStyles: function(!HTMLTemplateElement, string, string=),
 * prepareTemplateDom: function(!HTMLTemplateElement, string),
 * styleDocument: function(Object<string, string>=),
 * flushCustomStyles: function(),
 * getComputedStyleValue: function(!Element, string): string,
 * ScopingShim: (Object|undefined),
 * ApplyShim: (Object|undefined),
 * CustomStyleInterface: (Object|undefined),
 * nativeCss: boolean,
 * nativeShadow: boolean,
 * cssBuild: (string | undefined),
 * disableRuntime: boolean,
 * }}
 */
let ShadyCSSInterface; //eslint-disable-line no-unused-vars

/**
 * @typedef {{
 * shimcssproperties: (boolean | undefined),
 * shimshadow: (boolean | undefined),
 * cssBuild: (string | undefined),
 * disableRuntime: (boolean | undefined),
 * }}
 */
let ShadyCSSOptions; //eslint-disable-line no-unused-vars

/** @type {(ShadyCSSInterface | ShadyCSSOptions | undefined)} */
window.ShadyCSS;

/** @type {string|undefined} */
Element.prototype.extends;

/** @type {?Element|undefined} */
Element.prototype._element;

/** @type {string|undefined} */
Element.prototype.__cssBuild;

/** @type {boolean|undefined} */
HTMLTemplateElement.prototype._validating;

/** @type {boolean|undefined} */
HTMLTemplateElement.prototype._prepared;

/** @type {boolean|undefined} */
HTMLTemplateElement.prototype._domPrepared;

/** @type {?DocumentFragment|undefined} */
HTMLTemplateElement.prototype._content;

/** @type {?HTMLStyleElement|undefined} */
HTMLTemplateElement.prototype._gatheredStyle;

/** @type {?HTMLStyleElement|undefined} */
HTMLTemplateElement.prototype._style;

/**
 * @type {string | undefined}
 */
DOMTokenList.prototype.value;