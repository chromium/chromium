/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides a soy renderer that allows registration of
 * injected data ("globals") that will be passed into the rendered
 * templates.
 */

goog.module('goog.soy.Renderer');
goog.module.declareLegacyNamespace();

const InjectedDataSupplier = goog.requireType('goog.soy.InjectedDataSupplier');
const SafeHtml = goog.requireType('goog.html.SafeHtml');
const SafeStyleSheet = goog.requireType('goog.html.SafeStyleSheet');
const SanitizedContent = goog.require('goog.soy.data.SanitizedContent');
const SanitizedContentKind = goog.require('goog.soy.data.SanitizedContentKind');
const SanitizedCss = goog.requireType('goog.soy.data.SanitizedCss');
const SanitizedHtml = goog.requireType('goog.soy.data.SanitizedHtml');
const SanitizedUri = goog.requireType('goog.soy.data.SanitizedUri');
const asserts = goog.require('goog.asserts');
const dom = goog.require('goog.dom');
const soy = goog.require('goog.soy');

/**
 * Creates a new soy renderer. Note that the renderer will only be
 * guaranteed to work correctly within the document scope provided in
 * the DOM helper.
 */
class Renderer {
  /**
   * @param {?InjectedDataSupplier=} injectedDataSupplier A supplier that
   *     provides an injected data.
   * @param {?dom.DomHelper=} domHelper Optional DOM helper; defaults to that
   *     provided by `dom.getDomHelper()`.
   */
  constructor(injectedDataSupplier = undefined, domHelper = undefined) {
    /** @private @const {!dom.DomHelper} */
    this.dom_ = domHelper || dom.getDomHelper();

    /** @private @const {?InjectedDataSupplier} */
    this.supplier_ = injectedDataSupplier || null;
  }

  /**
   * Renders a Soy template into a single node or a document fragment.
   * Delegates to `soy.renderAsFragment`.
   * @param {function(ARG_TYPES, ?soy.CompatibleIj=): *} template The Soy
   *     template defining the element's content.
   * @param {ARG_TYPES=} templateData The data for the template.
   * @return {!Node} The resulting node or document fragment.
   * @template ARG_TYPES
   */
  renderAsFragment(template, templateData = undefined) {
    const node = soy.renderAsFragment(
        template, templateData, this.getInjectedData_(), this.dom_);
    this.handleRender(node, SanitizedContentKind.HTML);
    return node;
  }

  /**
   * Renders a Soy template into a single node. If the rendered HTML
   * string represents a single node, then that node is returned.
   * Otherwise, a DIV element is returned containing the rendered nodes.
   * Delegates to `soy.renderAsElement`.
   * @param {function(ARG_TYPES, ?soy.CompatibleIj=): *} template The Soy
   *     template defining the element's content.
   * @param {ARG_TYPES=} templateData The data for the template.
   * @return {!Element} Rendered template contents, wrapped in a parent DIV
   *     element if necessary.
   * @template ARG_TYPES
   */
  renderAsElement(template, templateData = undefined) {
    const element = soy.renderAsElement(
        template, templateData, this.getInjectedData_(), this.dom_);
    this.handleRender(element, SanitizedContentKind.HTML);
    return element;
  }

  /**
   * Renders a Soy template and then set the output string as the
   * innerHTML of the given element. Delegates to `soy.renderElement`.
   * @param {?Element} element The element whose content we are rendering.
   * @param {function(ARG_TYPES, ?soy.CompatibleIj=): *} template The Soy
   *     template defining the element's content.
   * @param {ARG_TYPES=} templateData The data for the template.
   * @template ARG_TYPES
   */
  renderElement(element, template, templateData = undefined) {
    soy.renderElement(element, template, templateData, this.getInjectedData_());
    this.handleRender(element, SanitizedContentKind.HTML);
  }

  /**
   * Renders a Soy template and returns the output string.
   * If the template is strict, it must be of kind HTML. To render strict
   * templates of other kinds, use `renderText` (for `kind="text"`) or
   * `renderStrictOfKind`.
   * @param {function(ARG_TYPES, ?Object<string, *>=): *} template The Soy
   *     template to render.
   * @param {ARG_TYPES=} templateData The data for the template.
   * @return {string} The return value of rendering the template directly.
   * @template ARG_TYPES
   */
  render(template, templateData = undefined) {
    const result = template(templateData || {}, this.getInjectedData_());
    asserts.assert(
        !(result instanceof SanitizedContent) ||
            result.contentKind === SanitizedContentKind.HTML,
        'render was called with a strict template of kind other than "html"' +
            ' (consider using renderText or renderStrict)');
    const contentKind =
        result instanceof SanitizedContent ? result.contentKind : null;
    this.handleRender(null /* node */, contentKind);
    return String(result);
  }

  /**
   * Renders a strict Soy template of kind="text" and returns the output string.
   * It is an error to use renderText on templates of kinds other than "text".
   * @param {function(ARG_TYPES, ?Object<string,*>=): string} template The Soy
   *     template to render.
   * @param {ARG_TYPES=} templateData The data for the template.
   * @return {string} The return value of rendering the template directly.
   * @template ARG_TYPES
   */
  renderText(template, templateData = undefined) {
    const result = template(templateData || {}, this.getInjectedData_());
    asserts.assertString(
        result,
        'renderText was called with a template of kind other than "text"');
    return String(result);
  }

  /**
   * Renders a strict Soy HTML template and returns the output SanitizedHtml
   * object.
   * @param {function(ARG_TYPES, ?Object<string,*>=):
   *     !SanitizedHtml} template The Soy template to render.
   * @param {ARG_TYPES=} templateData The data for the template.
   * @return {!SanitizedHtml}
   * @template ARG_TYPES
   */
  renderStrict(template, templateData = undefined) {
    return this.renderStrictOfKind(
        template, templateData, SanitizedContentKind.HTML);
  }

  /**
   * Renders a strict Soy template and returns the output SanitizedUri object.
   * @param {function(ARG_TYPES, ?Object<string, *>=):
   *     !SanitizedUri} template The Soy template to render.
   * @param {ARG_TYPES=} templateData The data for the template.
   * @return {!SanitizedUri}
   * @template ARG_TYPES
   */
  renderStrictUri(template, templateData = undefined) {
    return this.renderStrictOfKind(
        template, templateData, SanitizedContentKind.URI);
  }

  /**
   * Renders a strict Soy template and returns the output SanitizedContent
   * object.
   * @param {function(ARG_TYPES, ?Object<string, *>=): RETURN_TYPE} template The
   *     Soy template to render.
   * @param {ARG_TYPES=} templateData The data for the template.
   * @param {?SanitizedContentKind=} kind The output kind to assert. If null,
   *     the template must be of kind="html" (i.e., kind defaults to
   *     SanitizedContentKind.HTML).
   * @return {RETURN_TYPE} The SanitizedContent object. This return type is
   *     generic based on the return type of the template, such as
   *     soy.data.SanitizedHtml.
   * @template ARG_TYPES, RETURN_TYPE
   */
  renderStrictOfKind(template, templateData = undefined, kind = undefined) {
    const result = template(templateData || {}, this.getInjectedData_());
    asserts.assertInstanceof(
        result, SanitizedContent,
        'renderStrict cannot be called on a text soy template');
    asserts.assert(
        result.contentKind === (kind || SanitizedContentKind.HTML),
        'renderStrict was called with the wrong kind of template');
    this.handleRender(null /* node */, result.contentKind);
    return result;
  }

  /**
   * Renders a strict Soy template of kind="html" and returns the result as
   * a SafeHtml object.
   * Rendering a template that is not a strict template of kind="html" results
   * in a runtime error.
   * @param {function(ARG_TYPES, ?Object<string, *>=):
   *     !SanitizedHtml} template The Soy template to render.
   * @param {ARG_TYPES=} templateData The data for the template.
   * @return {!SafeHtml}
   * @template ARG_TYPES
   */
  renderSafeHtml(template, templateData = undefined) {
    const result = this.renderStrict(template, templateData);
    // Convert from SanitizedHtml to SafeHtml.
    return result.toSafeHtml();
  }

  /**
   * Renders a strict Soy template of kind="css" and returns the result as
   * a SafeStyleSheet object.
   * Rendering a template that is not a strict template of kind="css" results in
   * a runtime and compile-time error.
   * @param {function(ARG_TYPES, ?Object<string, *>=):
   *     !SanitizedCss} template The Soy template to render.
   * @param {ARG_TYPES=} templateData The data for the template.
   * @return {!SafeStyleSheet}
   * @template ARG_TYPES
   */
  renderSafeStyleSheet(template, templateData = undefined) {
    const result = this.renderStrictOfKind(
        template, templateData, SanitizedContentKind.CSS);
    return result.toSafeStyleSheet();
  }

  /**
   * @return {!dom.DomHelper}
   * @protected
   */
  getDom() {
    return this.dom_;
  }

  /**
   * Observes rendering of non-text templates by this renderer.
   * @param {?Node} node Relevant node, if available. The node may or may not be
   *     in the document, depending on whether Soy is creating an element or
   *     writing into an existing one.
   * @param {?SanitizedContentKind} kind of the template, or null if it was not
   *     strict.
   * @protected
   */
  handleRender(node, kind) {}

  /**
   * Creates the injectedParams map if necessary and calls the configuration
   * service to prepopulate it.
   * @return {?} The injected params.
   * @private
   */
  getInjectedData_() {
    return this.supplier_ ? this.supplier_.getData() : {};
  }
}

exports = Renderer;
