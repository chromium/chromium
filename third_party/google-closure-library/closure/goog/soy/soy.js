/**
 * @license Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides utility methods to render soy template.
 */
goog.module('goog.soy');
goog.module.declareLegacyNamespace();

const NodeType = goog.require('goog.dom.NodeType');
const SafeHtml = goog.require('goog.html.SafeHtml');
const SanitizedContent = goog.require('goog.soy.data.SanitizedContent');
const SanitizedHtml = goog.requireType('goog.soy.data.SanitizedHtml');
const TagName = goog.require('goog.dom.TagName');
const asserts = goog.require('goog.asserts');
const googDom = goog.require('goog.dom');
const safe = goog.require('goog.dom.safe');

/**
 * A structural interface for injected data.
 *
 * <p>Soy generated code contributes optional properties.
 * @record
 */
class IjData {}
exports.IjData = IjData;

/**
 * Helper typedef for ij parameters.  This is what soy generates.
 * @typedef {!IjData|!Object<string, *>}
 */
let CompatibleIj;
exports.CompatibleIj = CompatibleIj;

/**
 * Type definition for strict Soy templates. Very useful when passing a template
 * as an argument.
 * @typedef {function(?=, ?CompatibleIj=):(string|!SanitizedContent)}
 */
let StrictTemplate;
exports.StrictTemplate = StrictTemplate;

/**
 * Type definition for strict Soy HTML templates. Very useful when passing
 * a template as an argument.
 * @typedef {function(?=, ?CompatibleIj=):!SanitizedHtml}
 */
let StrictHtmlTemplate;
exports.StrictHtmlTemplate = StrictHtmlTemplate;

/**
 * Type definition for text templates.
 * @typedef {function(?=, ?CompatibleIj=):string}
 */
let TextTemplate;
exports.TextTemplate = TextTemplate;

/**
 * Sets the processed template as the innerHTML of an element. It is recommended
 * to use this helper function instead of directly setting innerHTML in your
 * hand-written code, so that it will be easier to audit the code for cross-site
 * scripting vulnerabilities.
 * @param {?Element|?ShadowRoot} element The element whose content we are
 *     rendering into.
 * @param {!SanitizedContent} templateResult The processed template of kind HTML
 *     or TEXT (which will be escaped).
 * @template ARG_TYPES
 */
function renderHtml(element, templateResult) {
  safe.unsafeSetInnerHtmlDoNotUseOrElse(
      asserts.assert(element), ensureTemplateOutputHtml(templateResult));
}
exports.renderHtml = renderHtml;

/**
 * Renders a Soy template and then set the output string as
 * the innerHTML of an element. It is recommended to use this helper function
 * instead of directly setting innerHTML in your hand-written code, so that it
 * will be easier to audit the code for cross-site scripting vulnerabilities.
 * @param {?Element|?ShadowRoot} element The element whose content we are
 *     rendering into.
 * @param {function(ARG_TYPES, ?CompatibleIj=): *} template The Soy
 *     template defining the element's content.
 * @param {ARG_TYPES=} templateData The data for the template.
 * @param {?Object=} injectedData The injected data for the template.
 * @template ARG_TYPES
 */
function renderElement(
    element, template, templateData = undefined, injectedData = undefined) {
  const html = ensureTemplateOutputHtml(
      template(templateData || defaultTemplateData, injectedData));
  safe.unsafeSetInnerHtmlDoNotUseOrElse(asserts.assert(element), html);
}
exports.renderElement = renderElement;

/**
 * Renders a Soy template into a single node or a document
 * fragment. If the rendered HTML string represents a single node, then that
 * node is returned (note that this is *not* a fragment, despite the name of the
 * method). Otherwise a document fragment is returned containing the rendered
 * nodes.
 * @param {function(ARG_TYPES, ?CompatibleIj=): *} template The Soy
 *     template defining the element's content. The kind of the template must be
 *     "html" or "text".
 * @param {ARG_TYPES=} templateData The data for the template.
 * @param {?Object=} injectedData The injected data for the template.
 * @param {?googDom.DomHelper=} domHelper The DOM helper used to create DOM
 *     nodes; defaults to `goog.dom.getDomHelper`.
 * @return {!Node} The resulting node or document fragment.
 * @template ARG_TYPES
 */
function renderAsFragment(
    template, templateData = undefined, injectedData = undefined,
    domHelper = undefined) {
  const dom = domHelper || googDom.getDomHelper();
  const output = template(templateData || defaultTemplateData, injectedData);
  const html = ensureTemplateOutputHtml(output);
  assertFirstTagValid(html.getTypedStringValue());
  return dom.safeHtmlToNode(html);
}
exports.renderAsFragment = renderAsFragment;

/**
 * Renders a Soy template into a single node. If the rendered
 * HTML string represents a single node, then that node is returned. Otherwise,
 * a DIV element is returned containing the rendered nodes.
 * @param {function(ARG_TYPES, ?CompatibleIj=): *} template The Soy
 *     template defining the element's content.
 * @param {ARG_TYPES=} templateData The data for the template.
 * @param {?Object=} injectedData The injected data for the template.
 * @param {?googDom.DomHelper=} domHelper The DOM helper used to create DOM
 *     nodes; defaults to `goog.dom.getDomHelper`.
 * @return {!Element} Rendered template contents, wrapped in a parent DIV
 *     element if necessary.
 * @template ARG_TYPES
 */
function renderAsElement(
    template, templateData = undefined, injectedData = undefined,
    domHelper = undefined) {
  return convertToElementInternal(
      template(templateData || defaultTemplateData, injectedData), domHelper);
}
exports.renderAsElement = renderAsElement;

/**
 * Converts a processed Soy template into a single node. If the rendered
 * HTML string represents a single node, then that node is returned. Otherwise,
 * a DIV element is returned containing the rendered nodes.
 * @param {!SanitizedContent} templateResult The processed template of kind HTML
 *     or TEXT (which will be escaped).
 * @param {?googDom.DomHelper=} domHelper The DOM helper used to create DOM
 *     nodes; defaults to `goog.dom.getDomHelper`.
 * @return {!Element} Rendered template contents, wrapped in a parent DIV
 *     element if necessary.
 */
function convertToElement(templateResult, domHelper = undefined) {
  return convertToElementInternal(templateResult, domHelper);
}
exports.convertToElement = convertToElement;

/**
 * Non-strict version of `convertToElement`.
 * @param {*} templateResult The processed template.
 * @param {?googDom.DomHelper=} domHelper The DOM helper used to create DOM
 *     nodes; defaults to `goog.dom.getDomHelper`.
 * @return {!Element} Rendered template contents, wrapped in a parent DIV
 *     element if necessary.
 */
function convertToElementInternal(templateResult, domHelper = undefined) {
  const dom = domHelper || googDom.getDomHelper();
  const wrapper = dom.createElement(TagName.DIV);
  const html = ensureTemplateOutputHtml(templateResult);
  assertFirstTagValid(html.getTypedStringValue());
  safe.unsafeSetInnerHtmlDoNotUseOrElse(wrapper, html);

  // If the template renders as a single element, return it.
  if (wrapper.childNodes.length == 1) {
    const firstChild = wrapper.firstChild;
    if (firstChild.nodeType == NodeType.ELEMENT) {
      return /** @type {!Element} */ (firstChild);
    }
  }

  // Otherwise, return the wrapper DIV.
  return wrapper;
}

/**
 * Ensures the result is "safe" to insert as HTML.
 * In the case the argument is a SanitizedContent object, it either must
 * already be of kind HTML, or if it is kind="text", the output will be HTML
 * escaped.
 * @param {*} templateResult The template result.
 * @return {!SafeHtml} The assumed-safe HTML output string.
 */
function ensureTemplateOutputHtml(templateResult) {
  // Note we allow everything that isn't an object, because some non-escaping
  // templates end up returning non-strings if their only print statement is a
  // non-escaped argument, plus some unit tests spoof templates.
  // TODO(gboyer): Track down and fix these cases.
  if (!goog.isObject(templateResult)) {
    return SafeHtml.htmlEscape(String(templateResult));
  }

  // Allow SanitizedContent of kind HTML.
  if (templateResult instanceof SanitizedContent) {
    return templateResult.toSafeHtml();
  }

  asserts.fail(
      `Soy template output is unsafe for use as HTML: ${templateResult}`);

  // In production, return a safe string, rather than failing hard.
  return SafeHtml.htmlEscape('zSoyz');
}

/**
 * Checks that the rendered HTML does not start with an invalid tag that would
 * likely cause unexpected output from renderAsElement or renderAsFragment.
 * See {@link http://www.w3.org/TR/html5/semantics.html#semantics} for reference
 * as to which HTML elements can be parents of each other.
 * @param {string} html The output of a template.
 */
function assertFirstTagValid(html) {
  if (asserts.ENABLE_ASSERTS) {
    const matches = html.match(INVALID_TAG_TO_RENDER);
    asserts.assert(
        !matches,
        'This template starts with a %s, which ' +
            'cannot be a child of a <div>, as required by soy internals. ' +
            'Consider using goog.soy.renderElement instead.\nTemplate output: %s',
        matches && matches[0], html);
  }
}

/**
 * A pattern to find templates that cannot be rendered by renderAsElement or
 * renderAsFragment, as these elements cannot exist as the child of a <div>.
 * @type {!RegExp}
 */
const INVALID_TAG_TO_RENDER =
    /^<(body|caption|col|colgroup|head|html|tr|td|th|tbody|thead|tfoot)>/i;

/**
 * Immutable object that is passed into templates that are rendered
 * without any data.
 * @const
 */
const defaultTemplateData = {};
