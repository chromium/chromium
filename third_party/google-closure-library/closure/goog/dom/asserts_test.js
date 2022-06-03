/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.assertsTest');
goog.setTestOnly();

const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const StrictMock = goog.require('goog.testing.StrictMock');
const asserts = goog.require('goog.dom.asserts');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let stubs;

testSuite({
  setUpPage() {
    stubs = new PropertyReplacer();
  },

  tearDown() {
    stubs.reset();
  },

  testAssertIsLocation() {
    assertNotThrows(() => {
      asserts.assertIsLocation(window.location);
    });

    // Ad-hoc mock objects are allowed.
    const o = {foo: 'bar'};
    assertNotThrows(() => {
      asserts.assertIsLocation(o);
    });

    // So are fancy mocks.
    const mock = new StrictMock(window.location);
    assertNotThrows(() => {
      asserts.assertIsLocation(mock);
    });

    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const linkElement = document.createElement('LINK');
      const ex = assertThrows(() => {
        asserts.assertIsLocation(linkElement);
      });
      assertContains('Argument is not a Location', ex.message);
    }
  },

  testAssertIsHtmlAnchorElement() {
    const anchorElement = document.createElement('A');
    assertNotThrows(() => {
      asserts.assertIsHTMLAnchorElement(anchorElement);
    });

    // Ad-hoc mock objects are allowed.
    const o = {foo: 'bar'};
    assertNotThrows(() => {
      asserts.assertIsHTMLAnchorElement(o);
    });
    // So are fancy mocks.
    const mock = new StrictMock(anchorElement);
    assertNotThrows(() => {
      asserts.assertIsHTMLAnchorElement(mock);
    });

    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('LINK');
      const ex = assertThrows(() => {
        asserts.assertIsHTMLAnchorElement(otherElement);
      });
      assertContains('Argument is not a HTMLAnchorElement', ex.message);
    }
  },

  testAssertIsHtmlButtonElement() {
    const buttonElement = document.createElement('BUTTON');
    assertNotThrows(() => {
      asserts.assertIsHTMLButtonElement(buttonElement);
    });

    // Ad-hoc mock objects are allowed.
    const o = {foo: 'bar'};
    assertNotThrows(() => {
      asserts.assertIsHTMLButtonElement(o);
    });
    // So are fancy mocks.
    const mock = new StrictMock(buttonElement);
    assertNotThrows(() => {
      asserts.assertIsHTMLButtonElement(mock);
    });

    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('LINK');
      const ex = assertThrows(() => {
        asserts.assertIsHTMLButtonElement(otherElement);
      });
      assertContains('Argument is not a HTMLButtonElement', ex.message);
    }
  },

  testAssertIsHtmlLinkElement() {
    const linkElement = document.createElement('LINK');
    assertNotThrows(() => {
      asserts.assertIsHTMLLinkElement(linkElement);
    });

    // Ad-hoc mock objects are allowed.
    const o = {foo: 'bar'};
    assertNotThrows(() => {
      asserts.assertIsHTMLLinkElement(o);
    });

    // So are fancy mocks.
    const mock = new StrictMock(linkElement);
    assertNotThrows(() => {
      asserts.assertIsHTMLLinkElement(mock);
    });

    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('A');
      const ex = assertThrows(() => {
        asserts.assertIsHTMLLinkElement(otherElement);
      });
      assertContains('Argument is not a HTMLLinkElement', ex.message);
    }
  },

  testAssertIsHtmlImageElement() {
    const imgElement = document.createElement('IMG');
    assertNotThrows(() => {
      asserts.assertIsHTMLImageElement(imgElement);
    });

    // Ad-hoc mock objects are allowed.
    const o = {foo: 'bar'};
    assertNotThrows(() => {
      asserts.assertIsHTMLImageElement(o);
    });

    // So are fancy mocks.
    const mock = new StrictMock(imgElement);
    assertNotThrows(() => {
      asserts.assertIsHTMLImageElement(mock);
    });

    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('SCRIPT');
      const ex = assertThrows(() => {
        asserts.assertIsHTMLImageElement(otherElement);
      });
      assertContains('Argument is not a HTMLImageElement', ex.message);
    }
  },

  testAssertIsHtmlInputElement() {
    const inputElement = document.createElement('INPUT');
    assertNotThrows(() => {
      asserts.assertIsHTMLInputElement(inputElement);
    });

    // Ad-hoc mock objects are allowed.
    const o = {foo: 'bar'};
    assertNotThrows(() => {
      asserts.assertIsHTMLInputElement(o);
    });
    // So are fancy mocks.
    const mock = new StrictMock(inputElement);
    assertNotThrows(() => {
      asserts.assertIsHTMLInputElement(mock);
    });

    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('LINK');
      const ex = assertThrows(() => {
        asserts.assertIsHTMLInputElement(otherElement);
      });
      assertContains('Argument is not a HTMLInputElement', ex.message);
    }
  },

  testAssertIsHtmlFormElement() {
    const formElement = document.createElement('FORM');
    assertNotThrows(() => {
      asserts.assertIsHTMLFormElement(formElement);
    });

    // Ad-hoc mock objects are allowed.
    const o = {foo: 'bar'};
    assertNotThrows(() => {
      asserts.assertIsHTMLFormElement(o);
    });
    // So are fancy mocks.
    const mock = new StrictMock(formElement);
    assertNotThrows(() => {
      asserts.assertIsHTMLFormElement(mock);
    });

    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('LINK');
      const ex = assertThrows(() => {
        asserts.assertIsHTMLFormElement(otherElement);
      });
      assertContains('Argument is not a HTMLFormElement', ex.message);
    }
  },

  testAssertIsHtmlEmbedElement() {
    const el = document.createElement('EMBED');
    assertNotThrows(() => {
      asserts.assertIsHTMLEmbedElement(el);
    });

    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('SCRIPT');
      const ex = assertThrows(() => {
        asserts.assertIsHTMLEmbedElement(otherElement);
      });
      assertContains('Argument is not a HTMLEmbedElement', ex.message);
    }
  },

  testAssertIsHtmlFrameElement() {
    const el = document.createElement('FRAME');
    assertNotThrows(() => {
      asserts.assertIsHTMLFrameElement(el);
    });

    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('SCRIPT');
      const ex = assertThrows(() => {
        asserts.assertIsHTMLFrameElement(otherElement);
      });
      assertContains('Argument is not a HTMLFrameElement', ex.message);
    }
  },

  testAssertIsHtmlIFrameElement() {
    const el = document.createElement('IFRAME');
    assertNotThrows(() => {
      asserts.assertIsHTMLIFrameElement(el);
    });

    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('SCRIPT');
      const ex = assertThrows(() => {
        asserts.assertIsHTMLIFrameElement(otherElement);
      });
      assertContains('Argument is not a HTMLIFrameElement', ex.message);
    }
  },

  testAssertIsHtmlObjectElement() {
    const el = document.createElement('OBJECT');
    assertNotThrows(() => {
      asserts.assertIsHTMLObjectElement(el);
    });

    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('SCRIPT');
      const ex = assertThrows(() => {
        asserts.assertIsHTMLObjectElement(otherElement);
      });
      assertContains('Argument is not a HTMLObjectElement', ex.message);
    }
  },

  testAssertIsHtmlScriptElement() {
    const el = document.createElement('SCRIPT');
    assertNotThrows(() => {
      asserts.assertIsHTMLScriptElement(el);
    });

    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('IMG');
      const ex = assertThrows(() => {
        asserts.assertIsHTMLScriptElement(otherElement);
      });
      assertContains('Argument is not a HTMLScriptElement', ex.message);
    }
  },

  testInOtherWindow() {
    const iframe = document.createElement('IFRAME');
    document.body.appendChild(iframe);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const el = iframe.contentWindow.document.createElement('SCRIPT');
    assertNotThrows(() => {
      asserts.assertIsHTMLScriptElement(el);
    });

    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const ex = assertThrows(() => {
        asserts.assertIsHTMLImageElement(el);
      });
      assertContains('Argument is not a HTMLImageElement', ex.message);
    }

    document.body.removeChild(iframe);
  },

  testAssertIsElementType() {
    stubs.set(asserts, 'getWindow_', () => null);
    assertNotThrows(() => {
      asserts.assertIsLocation(null);
      asserts.assertIsHTMLAnchorElement(null);
    });
  },
});
