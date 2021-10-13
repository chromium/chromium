/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.domTest');
goog.setTestOnly();

const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');
const testingDom = goog.require('goog.testing.dom');
const userAgent = goog.require('goog.userAgent');

function shouldRunTests() {
  // This test has not yet been updated to run on IE8. See b/2997682.
  return !userAgent.IE || userAgent.isVersionOrHigher(9);
}

let root;

/**
 * @suppress {strictMissingProperties} suppression added to enable type
 * checking
 */
function findNodeWithHierarchy() {
  // Test a more complicated hierarchy.
  root.innerHTML = '<div>a<p>b<span>c</span>d</p>e</div>';
  assertEquals(
      String(TagName.DIV),
      testingDom.findTextNode('a', root).parentNode.tagName);
  assertEquals(
      String(TagName.P), testingDom.findTextNode('b', root).parentNode.tagName);
  assertEquals(
      String(TagName.SPAN),
      testingDom.findTextNode('c', root).parentNode.tagName);
  assertEquals(
      String(TagName.P), testingDom.findTextNode('d', root).parentNode.tagName);
  assertEquals(
      String(TagName.DIV),
      testingDom.findTextNode('e', root).parentNode.tagName);
}

function setUpAssertHtmlMatches() {
  let tag1;
  let tag2;

  if (userAgent.EDGE_OR_IE) {
    tag1 = TagName.DIV;
  } else if (userAgent.WEBKIT) {
    tag1 = TagName.P;
    tag2 = TagName.BR;
  } else if (userAgent.GECKO) {
    tag1 = TagName.SPAN;
    tag2 = TagName.BR;
  }

  let parent = dom.createDom(TagName.DIV);
  root.appendChild(parent);
  parent.style.fontSize = '2em';
  parent.style.display = 'none';
  if (!userAgent.WEBKIT) {
    parent.appendChild(dom.createTextNode('NonWebKitText'));
  }

  if (tag1) {
    const e1 = dom.createDom(tag1);
    parent.appendChild(e1);
    parent = e1;
  }
  if (tag2) {
    parent.appendChild(dom.createDom(tag2));
  }
  parent.appendChild(dom.createTextNode('Text'));
  if (userAgent.WEBKIT) {
    root.firstChild.appendChild(dom.createTextNode('WebKitText'));
  }
}

testSuite({
  setUpPage() {
    root = dom.getElement('root');
  },

  setUp() {
    dom.removeChildren(root);
  },

  testFindNode() {
    // Test the easiest case.
    root.innerHTML = 'a<br>b';
    assertEquals(testingDom.findTextNode('a', root), root.firstChild);
    assertEquals(testingDom.findTextNode('b', root), root.lastChild);
    assertNull(testingDom.findTextNode('c', root));
  },

  testFindNodeDuplicate() {
    // Test duplicate.
    root.innerHTML = 'c<br>c';
    assertEquals(
        'Should return first duplicate', testingDom.findTextNode('c', root),
        root.firstChild);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAssertHtmlContentsMatch() {
    setUpAssertHtmlMatches();

    testingDom.assertHtmlContentsMatch(
        '<div style="display: none; font-size: 2em">' +
            '[[!WEBKIT]]NonWebKitText<div class="IE EDGE"><p class="WEBKIT">' +
            '<span class="GECKO"><br class="GECKO WEBKIT">Text</span></p></div>' +
            '</div>[[WEBKIT]]WebKitText',
        root);
  },

  testAssertHtmlMismatchText() {
    setUpAssertHtmlMatches();

    // Should fail due to mismatched text'
    const e =
        assertThrowsJsUnitException(/**
                                       @suppress {checkTypes} suppression added
                                       to enable type checking
                                     */
                                    () => {
                                      testingDom.assertHtmlContentsMatch(
                                          '<div style="display: none; font-size: 2em">' +
                                              '[[IE GECKO]]NonWebKitText<div class="IE"><p class="WEBKIT">' +
                                              '<span class="GECKO"><br class="GECKO WEBKIT">Bad</span></p></div>' +
                                              '</div>[[WEBKIT]]Extra',
                                          root);
                                    });
    assertContains('Text should match', e.message);
  },

  testAssertHtmlMismatchTag() {
    setUpAssertHtmlMatches();

    // Should fail due to mismatched tag
    const e =
        assertThrowsJsUnitException(/**
                                       @suppress {checkTypes} suppression added
                                       to enable type checking
                                     */
                                    () => {
                                      testingDom.assertHtmlContentsMatch(
                                          '<span style="display: none; font-size: 2em">' +
                                              '[[IE GECKO]]NonWebKitText<div class="IE"><p class="WEBKIT">' +
                                              '<span class="GECKO"><br class="GECKO WEBKIT">Text</span></p></div>' +
                                              '</span>[[WEBKIT]]Extra',
                                          root);
                                    });
    assertContains('Tag names should match', e.message);
  },

  testAssertHtmlMismatchStyle() {
    setUpAssertHtmlMatches();

    // Should fail due to mismatched style
    const e =
        assertThrowsJsUnitException(/**
                                       @suppress {checkTypes} suppression added
                                       to enable type checking
                                     */
                                    () => {
                                      testingDom.assertHtmlContentsMatch(
                                          '<div style="display: none; font-size: 3em">' +
                                              '[[IE GECKO]]NonWebKitText<div class="IE"><p class="WEBKIT">' +
                                              '<span class="GECKO"><br class="GECKO WEBKIT">Text</span></p></div>' +
                                              '</div>[[WEBKIT]]Extra',
                                          root);
                                    });
    assertContains('Should have same styles', e.message);
  },

  testAssertHtmlMismatchOptionalText() {
    setUpAssertHtmlMatches();

    // Should fail due to mismatched text
    const e =
        assertThrowsJsUnitException(/**
                                       @suppress {checkTypes} suppression added
                                       to enable type checking
                                     */
                                    () => {
                                      testingDom.assertHtmlContentsMatch(
                                          '<div style="display: none; font-size: 2em">' +
                                              '[[IE GECKO]]Bad<div class="IE"><p class="WEBKIT">' +
                                              '<span class="GECKO"><br class="GECKO WEBKIT">Text</span></p></div>' +
                                              '</div>[[WEBKIT]]Bad',
                                          root);
                                    });
    assertContains('Text should match', e.message);
  },

  testAssertHtmlMismatchExtraActualAfterText() {
    root.innerHTML = '<div>abc</div>def';

    // Should fail due to extra actual nodes
    const e = assertThrowsJsUnitException(/**
                                             @suppress {checkTypes} suppression
                                             added to enable type checking
                                           */
                                          () => {
                                            testingDom.assertHtmlContentsMatch(
                                                '<div>abc</div>', root);
                                          });
    assertContains('Finished expected HTML before', e.message);
  },

  testAssertHtmlMismatchExtraActualAfterElement() {
    root.innerHTML = '<br>def';

    // Should fail due to extra actual nodes
    const e = assertThrowsJsUnitException(/**
                                             @suppress {checkTypes} suppression
                                             added to enable type checking
                                           */
                                          () => {
                                            testingDom.assertHtmlContentsMatch(
                                                '<br>', root);
                                          });
    assertContains('Finished expected HTML before', e.message);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAssertHtmlMatchesWithSplitTextNodes() {
    root.appendChild(dom.createTextNode('1'));
    root.appendChild(dom.createTextNode('2'));
    root.appendChild(dom.createTextNode('3'));
    testingDom.assertHtmlContentsMatch('123', root);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAssertHtmlMatchesWithDifferentlyOrderedAttributes() {
    root.innerHTML = '<div foo="a" bar="b" class="className"></div>';

    testingDom.assertHtmlContentsMatch(
        '<div bar="b" class="className" foo="a"></div>', root, true);
  },

  testAssertHtmlMismatchWithDifferentNumberOfAttributes() {
    root.innerHTML = '<div foo="a" bar="b"></div>';

    const e =
        assertThrowsJsUnitException(/**
                                       @suppress {checkTypes} suppression added
                                       to enable type checking
                                     */
                                    () => {
                                      testingDom.assertHtmlContentsMatch(
                                          '<div foo="a"></div>', root, true);
                                    });
    assertContains('Unexpected attribute with name bar in element', e.message);
  },

  testAssertHtmlMismatchWithDifferentAttributeNames() {
    root.innerHTML = '<div foo="a" bar="b"></div>';

    const e = assertThrowsJsUnitException(/**
                                             @suppress {checkTypes} suppression
                                             added to enable type checking
                                           */
                                          () => {
                                            testingDom.assertHtmlContentsMatch(
                                                '<div foo="a" baz="b"></div>',
                                                root, true);
                                          });
    assertContains('Expected to find attribute with name baz', e.message);
  },

  testAssertHtmlMismatchWithDifferentClassNames() {
    root.innerHTML = '<div class="className1"></div>';

    const e =
        assertThrowsJsUnitException(/**
                                       @suppress {checkTypes} suppression added
                                       to enable type checking
                                     */
                                    () => {
                                      testingDom.assertHtmlContentsMatch(
                                          '<div class="className2"></div>',
                                          root, true);
                                    });
    assertContains(
        'Expected class was: className2, but actual class was: className1',
        e.message);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAssertHtmlMatchesWithClassNameAndUserAgentSpecified() {
    root.innerHTML =
        '<div>' + (userAgent.GECKO ? '<div class="foo"></div>' : '') + '</div>';

    testingDom.assertHtmlContentsMatch(
        '<div><div class="foo GECKO"></div></div>', root, true);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAssertHtmlMatchesWithClassesInDifferentOrder() {
    root.innerHTML = '<div class="class1 class2"></div>';

    testingDom.assertHtmlContentsMatch(
        '<div class="class2 class1"></div>', root, true);
  },

  testAssertHtmlMismatchWithDifferentAttributeValues() {
    root.innerHTML = '<div foo="b" bar="a"></div>';

    const e = assertThrowsJsUnitException(/**
                                             @suppress {checkTypes} suppression
                                             added to enable type checking
                                           */
                                          () => {
                                            testingDom.assertHtmlContentsMatch(
                                                '<div foo="a" bar="a"></div>',
                                                root, true);
                                          });
    assertContains('Expected attribute foo has a different value', e.message);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAssertHtmlMatchesWhenStrictAttributesIsFalse() {
    root.innerHTML = '<div foo="a" bar="b"></div>';

    testingDom.assertHtmlContentsMatch('<div foo="a"></div>', root);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAssertHtmlMatchesForMethodsAttribute1() {
    root.innerHTML = '<a methods="get"></a>';

    testingDom.assertHtmlContentsMatch('<a></a>', root);
    testingDom.assertHtmlContentsMatch('<a methods="get"></a>', root);
    testingDom.assertHtmlContentsMatch('<a methods="get"></a>', root, true);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAssertHtmlMatchesForMethodsAttribute2() {
    root.innerHTML = '<input></input>';

    testingDom.assertHtmlContentsMatch('<input></input>', root);
    testingDom.assertHtmlContentsMatch('<input></input>', root, true);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAssertHtmlMatchesForIdAttribute() {
    root.innerHTML = '<div id="foo"></div>';

    testingDom.assertHtmlContentsMatch('<div></div>', root);
    testingDom.assertHtmlContentsMatch('<div id="foo"></div>', root);
    testingDom.assertHtmlContentsMatch('<div id="foo"></div>', root, true);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAssertHtmlMatchesWhenIdIsNotSpecified1() {
    root.innerHTML = '<div id="someId"></div>';

    testingDom.assertHtmlContentsMatch('<div></div>', root);
  },

  testAssertHtmlMismatchWhenIdIsNotSpecified2() {
    root.innerHTML = '<div id="someId"></div>';

    const e = assertThrowsJsUnitException(/**
                                             @suppress {checkTypes} suppression
                                             added to enable type checking
                                           */
                                          () => {
                                            testingDom.assertHtmlContentsMatch(
                                                '<div></div>', root, true);
                                          });
    assertContains('Unexpected attribute with name id in element', e.message);
  },

  testAssertHtmlMismatchWhenIdIsSpecified() {
    root.innerHTML = '<div></div>';

    let e = assertThrowsJsUnitException(/**
                                           @suppress {checkTypes} suppression
                                           added to enable type checking
                                         */
                                        () => {
                                          testingDom.assertHtmlContentsMatch(
                                              '<div id="someId"></div>', root);
                                        });
    assertContains(
        'Expected to find attribute with name id, in element', e.message);

    e = assertThrowsJsUnitException(/**
                                       @suppress {checkTypes} suppression added
                                       to enable type checking
                                     */
                                    () => {
                                      testingDom.assertHtmlContentsMatch(
                                          '<div id="someId"></div>', root,
                                          true);
                                    });
    assertContains(
        'Expected to find attribute with name id, in element', e.message);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAssertHtmlMatchesWhenIdIsEmpty() {
    root.innerHTML = '<div></div>';

    testingDom.assertHtmlContentsMatch('<div></div>', root);
    testingDom.assertHtmlContentsMatch('<div></div>', root, true);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAssertHtmlMatchesWithDisabledAttribute() {
    const disabledShortest = '<input disabled="disabled">';
    const disabledShort = '<input disabled="">';
    const disabledLong = '<input disabled="disabled">';
    const enabled = '<input>';

    root.innerHTML = disabledLong;
    testingDom.assertHtmlContentsMatch(disabledShortest, root, true);
    testingDom.assertHtmlContentsMatch(disabledShort, root, true);
    testingDom.assertHtmlContentsMatch(disabledLong, root, true);

    // Should fail due to mismatched text
    const e = assertThrowsJsUnitException(/**
                                             @suppress {checkTypes} suppression
                                             added to enable type checking
                                           */
                                          () => {
                                            testingDom.assertHtmlContentsMatch(
                                                enabled, root, true);
                                          });
    // Attribute value mismatch in IE.
    // Unexpected attribute error in other browsers.
    assertContains('disabled', e.message);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAssertHtmlMatchesWithCheckedAttribute() {
    const checkedShortest = '<input type="radio" name="x" checked="checked">';
    const checkedShort = '<input type="radio" name="x" checked="">';
    const checkedLong = '<input type="radio" name="x" checked="checked">';
    const unchecked = '<input type="radio" name="x">';

    root.innerHTML = checkedLong;
    testingDom.assertHtmlContentsMatch(checkedShortest, root, true);
    testingDom.assertHtmlContentsMatch(checkedShort, root, true);
    testingDom.assertHtmlContentsMatch(checkedLong, root, true);
    if (!userAgent.IE) {
      // CHECKED attribute is ignored because it's among BAD_IE_ATTRIBUTES_.
      const e =
          assertThrowsJsUnitException(/**
                                         @suppress {checkTypes}
                                         suppression added to enable type
                                         checking
                                       */
                                      () => {
                                        testingDom.assertHtmlContentsMatch(
                                            unchecked, root, true);
                                      });
      assertContains('Unexpected attribute with name checked', e.message);
    }
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAssertHtmlMatchesWithWhitespace() {
    dom.removeChildren(root);
    root.appendChild(dom.createTextNode('  A  '));
    testingDom.assertHtmlContentsMatch('  A  ', root);

    dom.removeChildren(root);
    root.appendChild(dom.createTextNode('  A  '));
    root.appendChild(dom.createDom(TagName.SPAN, null, '  B  '));
    root.appendChild(dom.createTextNode('  C  '));
    testingDom.assertHtmlContentsMatch('  A  <span>  B  </span>  C  ', root);

    dom.removeChildren(root);
    root.appendChild(dom.createTextNode('  A'));
    root.appendChild(dom.createDom(TagName.SPAN, null, '  B'));
    root.appendChild(dom.createTextNode('  C'));
    testingDom.assertHtmlContentsMatch('  A<span>  B</span>  C', root);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAssertHtmlMatchesWithWhitespaceAndNesting() {
    dom.removeChildren(root);
    root.appendChild(dom.createDom(
        TagName.DIV, null, dom.createDom(TagName.B, null, '  A  '),
        dom.createDom(TagName.B, null, '  B  ')));
    root.appendChild(dom.createDom(
        TagName.DIV, null, dom.createDom(TagName.B, null, '  C  '),
        dom.createDom(TagName.B, null, '  D  ')));

    testingDom.assertHtmlContentsMatch(
        '<div><b>  A  </b><b>  B  </b></div>' +
            '<div><b>  C  </b><b>  D  </b></div>',
        root);

    dom.removeChildren(root);
    root.appendChild(dom.createDom(
        TagName.B, null,
        dom.createDom(
            TagName.B, null, dom.createDom(TagName.B, null, '  A  '))));
    root.appendChild(dom.createDom(TagName.B, null, '  B  '));

    testingDom.assertHtmlContentsMatch(
        '<b><b><b>  A  </b></b></b><b>  B  </b>', root);

    dom.removeChildren(root);
    root.appendChild(dom.createDom(
        TagName.DIV, null,
        dom.createDom(
            TagName.B, null, dom.createDom(TagName.B, null, '  A  '))));
    root.appendChild(dom.createDom(TagName.B, null, '  B  '));

    testingDom.assertHtmlContentsMatch(
        '<div><b><b>  A  </b></b></div><b>  B  </b>', root);

    root.innerHTML = '&nbsp;';
    testingDom.assertHtmlContentsMatch('&nbsp;', root);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAssertHtmlContentsMatchWithTemplate() {
    const template = '<template><p>foo</p></template>';
    root.innerHTML = template;
    testingDom.assertHtmlContentsMatch(template, root, true);
    assertThrowsJsUnitException(/**
                                   @suppress {checkTypes} suppression added to
                                   enable type checking
                                 */
                                () => {
                                  testingDom.assertHtmlContentsMatch(
                                      '<template><p id="bar">foo</p></template>',
                                      root, true);
                                });
    assertThrowsJsUnitException(/**
                                   @suppress {checkTypes} suppression added to
                                   enable type checking
                                 */
                                () => {
                                  testingDom.assertHtmlContentsMatch(
                                      '<template><p>bar</p></template>', root,
                                      true);
                                });
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAssertHtmlContentsMatchWithNestedTemplate() {
    const nestedTemplate =
        '<template><br><template><br></template><br></template>';
    root.innerHTML = nestedTemplate;
    testingDom.assertHtmlContentsMatch(nestedTemplate, root, true);
    assertThrowsJsUnitException(/**
                                   @suppress {checkTypes} suppression added to
                                   enable type checking
                                 */
                                () => {
                                  testingDom.assertHtmlContentsMatch(
                                      '<template><br><template id="foo"><br></template><br></template>',
                                      root, true);
                                });
    assertThrowsJsUnitException(/**
                                   @suppress {checkTypes} suppression added to
                                   enable type checking
                                 */
                                () => {
                                  testingDom.assertHtmlContentsMatch(
                                      '<template><br><template><br>bar</template><br></template>',
                                      root, true);
                                });
  },

  testAssertHtmlContentsMatchWithEmptyTemplate() {
    let template = '<template><p>foo</p></template>';
    root.innerHTML = template;
    assertThrowsJsUnitException(/**
                                   @suppress {checkTypes} suppression added to
                                   enable type checking
                                 */
                                () => {
                                  testingDom.assertHtmlContentsMatch(
                                      '<template></template>', root, true);
                                });

    template = '<template></template>';
    root.innerHTML = template;
    assertThrowsJsUnitException(/**
                                   @suppress {checkTypes} suppression added to
                                   enable type checking
                                 */
                                () => {
                                  testingDom.assertHtmlContentsMatch(
                                      '<template><p>bar</p></template>', root,
                                      true);
                                });
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAssertHtmlContentsMatchWithHttpCredentials() {
    const img = '<img src="http://foo:bar@example.com">';
    root.innerHTML = img;
    testingDom.assertHtmlContentsMatch(img, root, true);
    assertThrowsJsUnitException(/**
                                   @suppress {checkTypes} suppression added to
                                   enable type checking
                                 */
                                () => {
                                  testingDom.assertHtmlContentsMatch(
                                      '<img src="http://bar:baz@example.com">',
                                      root, true);
                                });
  },

  testAssertHtmlMatches() {
    // Since assertHtmlMatches is based on assertHtmlContentsMatch, we leave the
    // majority of edge case testing to the above.  Here we just do a sanity
    // check.
    testingDom.assertHtmlMatches('<div>abc</div>', '<div>abc</div>');
    testingDom.assertHtmlMatches('<div>abc</div>', '<div>abc</div> ');
    testingDom.assertHtmlMatches(
        '<div style="font-size: 1px; color: red">abc</div>',
        '<div style="color: red;  font-size: 1px;;">abc</div>');

    // Should fail due to mismatched text
    const e = assertThrowsJsUnitException(() => {
      testingDom.assertHtmlMatches('<div>abc</div>', '<div>abd</div>');
    });
    assertContains('Text should match', e.message);
  },

  testAssertHtmlMatchesWithSvgAttributes() {
    testingDom.assertHtmlMatches(
        '<svg height="10px"></svg>', '<svg height="10px"></svg>');
  },

  testAssertHtmlMatchesWithScriptWithNewLines() {
    testingDom.assertHtmlMatches(
        '<script>var a;\nvar b;</script>', '<script>var a;\nvar b;</script>');
  },

  testAssertHtmlMatches_namespace() {
    testingDom.assertHtmlMatches(
        '<svg width="1px"></svg>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="1px"></svg>');
  },
});
