/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.soy.RendererTest');
goog.setTestOnly();

const Dir = goog.require('goog.i18n.bidi.Dir');
const NodeType = goog.require('goog.dom.NodeType');
const Renderer = goog.require('goog.soy.Renderer');
const SafeHtml = goog.require('goog.html.SafeHtml');
const SanitizedContentKind = goog.require('goog.soy.data.SanitizedContentKind');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const recordFunction = goog.require('goog.testing.recordFunction');
/** @suppress {extraRequire} */
const testHelper = goog.require('goog.soy.testHelper');
const testSuite = goog.require('goog.testing.testSuite');

let handleRender;

const dataSupplier = {
  getData: function() {
    return {name: 'IjValue'};
  }
};

testSuite({
  setUp() {
    // Replace the empty default implementation.
    /** @suppress {visibility} suppression added to enable type checking */
    handleRender = Renderer.prototype.handleRender =
        recordFunction(Renderer.prototype.handleRender);
  },

  testRenderElement() {
    const testDiv = dom.createElement(TagName.DIV);

    /** @suppress {checkTypes} suppression added to enable type checking */
    const renderer = new Renderer(dataSupplier);
    renderer.renderElement(
        testDiv, example.injectedDataTemplate, {name: 'Value'});
    assertEquals('ValueIjValue', elementToInnerHtml(testDiv));
    assertEquals(testDiv, handleRender.getLastCall().getArguments()[0]);
    handleRender.assertCallCount(1);
  },

  testRenderElementWithNoTemplateData() {
    const testDiv = dom.createElement(TagName.DIV);

    /** @suppress {checkTypes} suppression added to enable type checking */
    const renderer = new Renderer(dataSupplier);
    renderer.renderElement(testDiv, example.noDataTemplate);
    assertEquals('<div>Hello</div>', elementToInnerHtml(testDiv));
    assertEquals(testDiv, handleRender.getLastCall().getArguments()[0]);
    handleRender.assertCallCount(1);
  },

  testRenderAsFragment() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const renderer = new Renderer(dataSupplier);
    const fragment = renderer.renderAsFragment(
        example.injectedDataTemplate, {name: 'Value'});
    assertEquals('ValueIjValue', fragmentToHtml(fragment));
    assertEquals(fragment, handleRender.getLastCall().getArguments()[0]);
    handleRender.assertCallCount(1);
  },

  testRenderAsFragmentWithNoTemplateData() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const renderer = new Renderer(dataSupplier);
    const fragment = renderer.renderAsFragment(example.noDataTemplate);
    assertEquals(NodeType.ELEMENT, fragment.nodeType);
    assertEquals('<div>Hello</div>', fragmentToHtml(fragment));
    assertEquals(fragment, handleRender.getLastCall().getArguments()[0]);
    handleRender.assertCallCount(1);
  },

  testRenderAsElement() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const renderer = new Renderer(dataSupplier);
    const element =
        renderer.renderAsElement(example.injectedDataTemplate, {name: 'Value'});
    assertEquals('ValueIjValue', elementToInnerHtml(element));
    assertEquals(element, handleRender.getLastCall().getArguments()[0]);
    handleRender.assertCallCount(1);
  },

  testRenderAsElementWithNoTemplateData() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const renderer = new Renderer(dataSupplier);
    const elem = renderer.renderAsElement(example.noDataTemplate);
    assertEquals('Hello', elementToInnerHtml(elem));
    assertEquals(elem, handleRender.getLastCall().getArguments()[0]);
  },

  testRenderConvertsToString() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const renderer = new Renderer(dataSupplier);
    assertEquals(
        'Output should be a string', 'Hello <b>World</b>',
        renderer.render(example.sanitizedHtmlTemplate));
    assertNull(handleRender.getLastCall().getArguments()[0]);
    handleRender.assertCallCount(1);
  },

  testRenderRejectsNonHtmlStrictTemplates() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const renderer = new Renderer(dataSupplier);
    assertEquals(
        'Assertion failed: ' +
            'render was called with a strict template of kind other than "html"' +
            ' (consider using renderText or renderStrict)',
        assertThrows(/**
                        @suppress {checkTypes} suppression added to enable type
                        checking
                      */
                     () => {
                       renderer.render(example.sanitizedUriTemplate, {});
                     })
            .message);
    handleRender.assertCallCount(0);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRenderStrictDoesNotConvertToString() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const renderer = new Renderer(dataSupplier);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = renderer.renderStrict(example.sanitizedHtmlTemplate);
    assertEquals('Hello <b>World</b>', result.content);
    assertEquals(SanitizedContentKind.HTML, result.contentKind);
    assertNull(handleRender.getLastCall().getArguments()[0]);
    handleRender.assertCallCount(1);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testRenderStrictValidatesOutput() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const renderer = new Renderer(dataSupplier);
    // Passes.
    renderer.renderStrict(example.sanitizedHtmlTemplate, {});
    // No SanitizedContent at all.
    assertEquals(
        'Assertion failed: ' +
            'renderStrict cannot be called on a text soy template',
        assertThrows(/**
                        @suppress {checkTypes} suppression added to enable type
                        checking
                      */
                     () => {
                       renderer.renderStrict(example.stringTemplate, {});
                     })
            .message);
    assertNull(handleRender.getLastCall().getArguments()[0]);
    // Passes.
    renderer.renderStrictOfKind(
        example.sanitizedHtmlTemplate, {}, SanitizedContentKind.HTML);
    // Wrong content kind.
    assertEquals(
        'Assertion failed: ' +
            'renderStrict was called with the wrong kind of template',
        assertThrows(/**
                        @suppress {checkTypes} suppression added to enable type
                        checking
                      */
                     () => {
                       renderer.renderStrictOfKind(
                           example.sanitizedHtmlTemplate, {},
                           SanitizedContentKind.JS);
                     })
            .message);
    assertNull(handleRender.getLastCall().getArguments()[0]);

    // Rendering non-HTML template fails:
    assertEquals(
        'Assertion failed: ' +
            'renderStrict was called with the wrong kind of template',
        assertThrows(/**
                        @suppress {checkTypes} suppression added to enable type
                        checking
                      */
                     () => {
                       renderer.renderStrict(example.sanitizedUriTemplate, {});
                     })
            .message);
    assertNull(handleRender.getLastCall().getArguments()[0]);
    handleRender.assertCallCount(2);
  },

  testRenderStrictUri() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const renderer = new Renderer(dataSupplier);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = renderer.renderStrictUri(example.sanitizedUriTemplate, {});
    assertEquals(SanitizedContentKind.URI, result.contentKind);
    assertEquals(
        'Assertion failed: ' +
            'renderStrict was called with the wrong kind of template',
        assertThrows(/**
                        @suppress {checkTypes} suppression added to enable type
                        checking
                      */
                     () => {
                       renderer.renderStrictUri(
                           example.sanitizedHtmlTemplate, {});
                     })
            .message);
    handleRender.assertCallCount(1);
  },

  testRenderText() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const renderer = new Renderer(dataSupplier);
    // RenderText works on string templates.
    assertEquals('<b>XSS</b>', renderer.renderText(example.stringTemplate));
    // RenderText on non-text template fails.
    assertEquals(
        'Assertion failed: ' +
            'renderText was called with a template of kind other than "text"',
        assertThrows(/**
                        @suppress {checkTypes} suppression added to enable type
                        checking
                      */
                     () => {
                       renderer.renderText(example.sanitizedHtmlTemplate, {});
                     })
            .message);
  },

  testRenderSafeHtml() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const renderer = new Renderer(dataSupplier);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = renderer.renderSafeHtml(example.sanitizedHtmlTemplate);
    assertEquals('Hello <b>World</b>', SafeHtml.unwrap(result));
    assertEquals(Dir.LTR, result.getDirection());
  },
});
