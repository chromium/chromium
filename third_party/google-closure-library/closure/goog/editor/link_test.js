/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.LinkTest');
goog.setTestOnly();

const Link = goog.require('goog.editor.Link');
const NodeType = goog.require('goog.dom.NodeType');
const Range = goog.require('goog.dom.Range');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');


let anchor;

testSuite({
  setUp() {
    anchor = dom.createDom(TagName.A);
    document.body.appendChild(anchor);
  },

  tearDown() {
    dom.removeNode(anchor);
  },

  testCreateNew() {
    const link = new Link(anchor, true);
    assertNotNull('Should have created object', link);
    assertTrue('Should be new', link.isNew());
    assertEquals('Should have correct anchor', anchor, link.getAnchor());
    assertEquals('Should be empty', '', link.getCurrentText());
  },

  testCreateNotNew() {
    const link = new Link(anchor, false);
    assertNotNull('Should have created object', link);
    assertFalse('Should not be new', link.isNew());
    assertEquals('Should have correct anchor', anchor, link.getAnchor());
    assertEquals('Should be empty', '', link.getCurrentText());
  },

  testCreateNewLinkFromText() {
    const url = 'http://www.google.com/';
    anchor.innerHTML = url;
    const link = Link.createNewLinkFromText(anchor);
    assertNotNull('Should have created object', link);
    assertEquals('Should have url in anchor', url, anchor.href);
  },

  testCreateNewLinkFromTextLeadingTrailingWhitespace() {
    const url = 'http://www.google.com/';
    const urlWithSpaces = ` ${url} `;
    anchor.innerHTML = urlWithSpaces;
    const urlWithSpacesUpdatedByBrowser = anchor.innerHTML;
    const link = Link.createNewLinkFromText(anchor);
    assertNotNull('Should have created object', link);
    assertEquals('Should have url in anchor', url, anchor.href);
    assertEquals(
        'The text should still have spaces', urlWithSpacesUpdatedByBrowser,
        link.getCurrentText());
  },

  testCreateNewLinkFromTextWithAnchor() {
    const url = 'https://www.google.com/';
    anchor.innerHTML = url;
    const link = Link.createNewLinkFromText(anchor, '_blank');
    assertNotNull('Should have created object', link);
    assertEquals('Should have url in anchor', url, anchor.href);
    assertEquals('Should have _blank target', '_blank', anchor.target);
  },

  testInitialize() {
    const link = Link.createNewLink(anchor, 'http://www.google.com');
    assertNotNull('Should have created object', link);
    assertTrue('Should be new', link.isNew());
    assertEquals('Should have correct anchor', anchor, link.getAnchor());
    assertEquals('Should be empty', '', link.getCurrentText());
  },

  testInitializeWithTarget() {
    const link = Link.createNewLink(anchor, 'http://www.google.com', '_blank');
    assertNotNull('Should have created object', link);
    assertTrue('Should be new', link.isNew());
    assertEquals('Should have correct anchor', anchor, link.getAnchor());
    assertEquals('Should be empty', '', link.getCurrentText());
    assertEquals('Should have _blank target', '_blank', anchor.target);
  },

  testSetText() {
    const link = Link.createNewLink(anchor, 'http://www.google.com', '_blank');
    assertEquals('Should be empty', '', link.getCurrentText());
    link.setTextAndUrl('Text', 'http://docs.google.com/');
    assertEquals(
        'Should point to http://docs.google.com/', 'http://docs.google.com/',
        anchor.href);
    assertEquals('Should have correct text', 'Text', link.getCurrentText());
  },

  testSetBoldText() {
    anchor.innerHTML = '<b></b>';
    const link = Link.createNewLink(anchor, 'http://www.google.com', '_blank');
    assertEquals('Should be empty', '', link.getCurrentText());
    link.setTextAndUrl('Text', 'http://docs.google.com/');
    assertEquals(
        'Should point to http://docs.google.com/', 'http://docs.google.com/',
        anchor.href);
    assertEquals('Should have correct text', 'Text', link.getCurrentText());
    assertEquals(
        'Should still be bold', String(TagName.B), anchor.firstChild.tagName);
  },

  testLinkImgTag() {
    anchor.innerHTML = '<img src="www.google.com" alt="alt_txt">';
    const link = Link.createNewLink(anchor, 'http://www.google.com', '_blank');
    assertEquals('Test getCurrentText', 'alt_txt', link.getCurrentText());
    link.setTextAndUrl('newText', 'http://docs.google.com/');
    assertEquals('Test getCurrentText', 'newText', link.getCurrentText());
    assertEquals(
        'Should point to http://docs.google.com/', 'http://docs.google.com/',
        anchor.href);

    assertEquals(
        'Should still have img tag', String(TagName.IMG),
        anchor.firstChild.tagName);

    assertEquals(
        'Alt should equal "newText"', 'newText',
        anchor.firstChild.getAttribute('alt'));
  },

  testLinkImgTagWithNoAlt() {
    anchor.innerHTML = '<img src="www.google.com">';
    const link = Link.createNewLink(anchor, 'http://www.google.com', '_blank');
    assertEquals('Test getCurrentText', '', link.getCurrentText());
  },

  testSetMixed() {
    anchor.innerHTML = '<b>A</b>B';
    const link = Link.createNewLink(anchor, 'http://www.google.com', '_blank');
    assertEquals('Should have text: AB', 'AB', link.getCurrentText());
    link.setTextAndUrl('Text', 'http://docs.google.com/');
    assertEquals(
        'Should point to http://docs.google.com/', 'http://docs.google.com/',
        anchor.href);
    assertEquals('Should have correct text', 'Text', link.getCurrentText());
    assertEquals(
        'Should not be bold', NodeType.TEXT, anchor.firstChild.nodeType);
  },

  testPlaceCursorRightOf() {
    // IE can only do selections properly if the region is editable.
    const ed = dom.createDom(TagName.DIV);
    dom.replaceNode(ed, anchor);
    ed.contentEditable = true;
    ed.appendChild(anchor);

    // In order to test the cursor placement properly, we need to have
    // link text.  See more details in the test below.
    dom.setTextContent(anchor, 'I am text');

    const link = Link.createNewLink(anchor, 'http://www.google.com');
    link.placeCursorRightOf();

    const range = Range.createFromWindow();
    assertTrue('Range should be collapsed', range.isCollapsed());
    const startNode = range.getStartNode();

    // Check that the selection is the "right" place.
    //
    // If you query the selection, it is actually still inside the anchor,
    // but if you type, it types outside the anchor.
    //
    // Best we can do is test that it is at the end of the anchor text.
    assertEquals(
        'Selection should be in anchor text', anchor.firstChild, startNode);
    assertEquals(
        'Selection should be at the end of the text', anchor.firstChild.length,
        range.getStartOffset());

    if (ed) {
      dom.removeNode(ed);
    }
  },

  testIsLikelyUrl() {
    const good = [
      // Proper URLs
      'http://google.com',
      'http://google.com/',
      'http://192.168.1.103',
      'http://www.google.com:8083',
      'https://antoine',
      'https://foo.foo.net',
      'ftp://google.com:22/',
      'http://user@site.com',
      'ftp://user:pass@ftp.site.com',
      'http://google.com/search?q=laser%20cats',
      'aim:goim?screenname=en2es',
      'mailto:x@y.com',

      // Bad URLs a browser will accept
      'www.google.com',
      'www.amazon.co.uk',
      'amazon.co.uk',
      'foo2.foo3.com',
      'pandora.tv',
      'marketing.us',
      'del.icio.us',
      'bridge-line.com',
      'www.frigid.net:80',
      'www.google.com?q=foo',
      'www.foo.com/j%20.txt',
      'foodtv.net',
      'google.com',
      'slashdot.org',
      '192.168.1.1',
      'justin.edu?kumar&nbsp;something',
      'google.com/search?q=hot%20pockets',

      // Due to TLD explosion, these could be URLs either now or soon.
      'ww.jester',
      'juicer.fake',
      'abs.nonsense.something',
      'filename.txt',
    ];
    let i;
    for (i = 0; i < good.length; i++) {
      assertTrue(good[i] + ' should be good', Link.isLikelyUrl(good[i]));
    }

    const bad = [
      // Definitely not URLs
      'bananas',
      'http google com',
      '<img>',
      'Sad :/',
      '*garbage!.123',
      'ftp',
      'http',
      '/',
      'https',
      'this is',
      '*!&.banana!*&!',
      'www.jester is gone.com',
      'ftp .nospaces.net',
      'www_foo_net',
      'www.\'jester\'.net',
      'www:8080',
      'www . notnsense.com',
      'email@address.com',
      '.',
      'x.',

      // URL-ish but not quite
      '  http://www.google.com',
      'http://www.google.com:8081   ',
      'www.google.com foo bar',
      'google.com/search?q=not quite',
    ];

    for (i = 0; i < bad.length; i++) {
      assertFalse(bad[i] + ' should be bad', Link.isLikelyUrl(bad[i]));
    }
  },

  testIsLikelyEmailAddress() {
    const good = [
      // Valid email addresses
      'foo@foo.com',
      'foo1@foo2.foo3.com',
      'f45_1@goog13.org',
      'user@gmail.co.uk',
      'jon-smith@crazy.net',
      'roland1@capuchino.gov',
      'ernir@gshi.nl',
      'JOON@jno.COM',
      'media@meDIa.fREnology.FR',
      'john.mail4me@del.icio.us',
      'www9@wc3.madeup1.org',
      'hi@192.168.1.103',
      'hi@192.168.1.1',
    ];
    let i;
    for (i = 0; i < good.length; i++) {
      assertTrue(Link.isLikelyEmailAddress(good[i]));
    }

    const bad = [
      // Malformed/incomplete email addresses
      'user',
      '@gmail.com',
      'user@gmail',
      'user@.com',
      'user@gmail.c',
      'user@gmail.co.u',
      '@ya.com',
      '.@hi3.nl',
      'jim.com',
      'ed:@gmail.com',
      '*!&.banana!*&!',
      ':jon@gmail.com',
      '3g?@bil.com',
      'adam be@hi.net',
      'john\nsmith@test.com',
      'www.\'jester\'.net',
      '\'james\'@covald.net',
      'ftp://user@site.com/',
      'aim:goim?screenname=en2es',
      'user:pass@site.com',
      'user@site.com yay',
    ];

    for (i = 0; i < bad.length; i++) {
      assertFalse(Link.isLikelyEmailAddress(bad[i]));
    }
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testIsMailToLink() {
    assertFalse(Link.isMailto());
    assertFalse(Link.isMailto(null));
    assertFalse(Link.isMailto(''));
    assertFalse(Link.isMailto('http://foo.com'));
    assertFalse(Link.isMailto('http://mailto:80'));

    assertTrue(Link.isMailto('mailto:'));
    assertTrue(Link.isMailto('mailto://'));
    assertTrue(Link.isMailto('mailto://ptucker@gmail.com'));
  },

  testGetValidLinkFromText() {
    const textLinkPairs = [
      // input text, expected link output
      'www.foo.com',
      'http://www.foo.com',
      'user@gmail.com',
      'mailto:user@gmail.com',
      'http://www.foo.com',
      'http://www.foo.com',
      'https://this.that.edu',
      'https://this.that.edu',
      'nothing to see here',
      null,
    ];
    const link = new Link(anchor, true);

    for (let i = 0; i < textLinkPairs.length; i += 2) {
      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      link.currentText_ = textLinkPairs[i];
      const result = link.getValidLinkFromText();
      assertEquals(textLinkPairs[i + 1], result);
    }
  },
});
