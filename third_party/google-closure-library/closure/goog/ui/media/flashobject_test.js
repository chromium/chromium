// Copyright 2009 The Closure Library Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

goog.module('goog.ui.media.FlashObjectTest');
goog.setTestOnly();

const DomHelper = goog.require('goog.dom.DomHelper');
const EventType = goog.require('goog.events.EventType');
const FlashObject = goog.require('goog.ui.media.FlashObject');
const GoogEvent = goog.require('goog.events.Event');
const MockControl = goog.require('goog.testing.MockControl');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.html.testing');
const testingEvents = goog.require('goog.testing.events');
const userAgent = goog.require('goog.userAgent');

// Delay running the tests after page load. This test has some asynchronous
// behavior that interacts with page load detection.
goog.testing.jsunit.AUTO_RUN_DELAY_IN_MS = 500;

const FLASH_URL = testing.newTrustedResourceUrlForTest(
    'http://www.youtube.com/v/RbI7cCp0v6w&hl=en&fs=1');
const control = new MockControl();
const domHelper = control.createLooseMock(DomHelper);
// TODO(user): mocking window.document throws exceptions in FF2. find out how
// to mock it.
const documentHelper = {
  body: control.createLooseMock(DomHelper)
};
const element = dom.createElement(TagName.DIV);

function getFlashVarsFromElement(flash) {
  let el = flash.getFlashElement();

  // This should work in everything except IE:
  if (el.hasAttribute && el.hasAttribute('flashvars'))
    return el.getAttribute('flashvars');

  // For IE: find and return the value of the correct param element:
  el = el.firstChild;
  while (el) {
    if (el.name == 'FlashVars') {
      return el.value;
    }
    el = el.nextSibling;
  }
  return '';
}

function assertContainsParam(element, expectedName, expectedValue) {
  const failureMsg = `Expected param with name "${expectedName}` +
      '\" and value \"' + expectedValue +
      '\". Not found in child nodes: ' + element.innerHTML;
  for (let i = 0; i < element.childNodes.length; i++) {
    const child = element.childNodes[i];
    const name = child.getAttribute('name');
    if (name === expectedName) {
      if (!child.getAttribute('value') === expectedValue) {
        fail(failureMsg);
      }
      return;
    }
  }
  fail(failureMsg);
}

testSuite({
  setUp() {
    control.$resetAll();
    domHelper.getDocument().$returns(documentHelper).$anyTimes();
    domHelper.createElement(TagName.DIV).$returns(element).$anyTimes();
    documentHelper.body.appendChild(element).$anyTimes();
  },

  tearDown() {
    control.$verifyAll();
  },

  testInstantiationAndRendering() {
    control.$replayAll();

    const flash = new FlashObject(FLASH_URL, domHelper);
    flash.render();
    flash.dispose();
  },

  testRenderedWithCorrectAttributes() {
    if (userAgent.IE && !userAgent.isDocumentModeOrHigher(11)) {
      return;
    }

    control.$replayAll();

    const flash = new FlashObject(FLASH_URL, domHelper);
    flash.setAllowScriptAccess('allowScriptAccess');
    flash.setBackgroundColor('backgroundColor');
    flash.setId('id');
    flash.setFlashVars({'k1': 'v1', 'k2': 'v2'});
    flash.setWmode('wmode');
    flash.render();

    const el = flash.getFlashElement();
    assertEquals('true', el.getAttribute('allowFullScreen'));
    assertEquals('all', el.getAttribute('allowNetworking'));
    assertEquals('allowScriptAccess', el.getAttribute('allowScriptAccess'));
    assertEquals(FlashObject.FLASH_CSS_CLASS, el.getAttribute('class'));
    assertEquals('k1=v1&k2=v2', el.getAttribute('FlashVars'));
    assertEquals('id', el.getAttribute('id'));
    assertEquals('id', el.getAttribute('name'));
    assertEquals(
        'https://www.macromedia.com/go/getflashplayer',
        el.getAttribute('pluginspage'));
    assertEquals('high', el.getAttribute('quality'));
    assertEquals('false', el.getAttribute('SeamlessTabbing'));
    assertEquals(FLASH_URL.getTypedStringValue(), el.getAttribute('src'));
    assertEquals('application/x-shockwave-flash', el.getAttribute('type'));
    assertEquals('wmode', el.getAttribute('wmode'));
  },

  testRenderedWithCorrectAttributesOldIe() {
    if (!userAgent.IE || userAgent.isDocumentModeOrHigher(11)) {
      return;
    }

    control.$replayAll();

    const flash = new FlashObject(FLASH_URL, domHelper);
    flash.setAllowScriptAccess('allowScriptAccess');
    flash.setBackgroundColor('backgroundColor');
    flash.setId('id');
    flash.setFlashVars({'k1': 'v1', 'k2': 'v2'});
    flash.setWmode('wmode');
    flash.render();

    const el = flash.getFlashElement();
    assertEquals(
        'class', FlashObject.FLASH_CSS_CLASS, el.getAttribute('class'));
    assertEquals(
        'clsid:d27cdb6e-ae6d-11cf-96b8-444553540000',
        el.getAttribute('classid'));
    assertEquals('id', 'id', el.getAttribute('id'));
    assertEquals('name', 'id', el.getAttribute('name'));

    assertContainsParam(el, 'allowFullScreen', 'true');
    assertContainsParam(el, 'allowNetworking', 'all');
    assertContainsParam(el, 'AllowScriptAccess', 'allowScriptAccess');
    assertContainsParam(el, 'bgcolor', 'backgroundColor');
    assertContainsParam(el, 'FlashVars', 'FlashVars');
    assertContainsParam(el, 'movie', FLASH_URL);
    assertContainsParam(el, 'quality', 'high');
    assertContainsParam(el, 'SeamlessTabbing', 'false');
    assertContainsParam(el, 'wmode', 'wmode');
  },

  testSetFlashVar() {
    control.$replayAll();

    const flash = new FlashObject(FLASH_URL, domHelper);

    assertTrue(flash.getFlashVars().isEmpty());
    flash.setFlashVar('foo', 'bar');
    flash.setFlashVar('hello', 'world');
    assertFalse(flash.getFlashVars().isEmpty());

    flash.render();

    assertEquals('foo=bar&hello=world', getFlashVarsFromElement(flash));
    flash.dispose();
  },

  testAddFlashVars() {
    control.$replayAll();

    const flash = new FlashObject(FLASH_URL, domHelper);

    assertTrue(flash.getFlashVars().isEmpty());
    flash.addFlashVars({'using': 'an', 'object': 'literal'});
    assertFalse(flash.getFlashVars().isEmpty());

    flash.render();

    assertEquals('using=an&object=literal', getFlashVarsFromElement(flash));
    flash.dispose();
  },

  /** @deprecated Remove once setFlashVars is removed. */
  testSetFlashVarsUsingFalseAsTheValue() {
    control.$replayAll();

    const flash = new FlashObject(FLASH_URL, domHelper);

    assertTrue(flash.getFlashVars().isEmpty());
    flash.setFlashVars('beEvil', false);
    assertFalse(flash.getFlashVars().isEmpty());

    flash.render();

    assertEquals('beEvil=false', getFlashVarsFromElement(flash));
    flash.dispose();
  },

  /** @deprecated Remove once setFlashVars is removed. */
  testSetFlashVarsWithWrongArgument() {
    control.$replayAll();

    assertThrows(() => {
      const flash = new FlashObject(FLASH_URL, domHelper);
      flash.setFlashVars('foo=bar');
      flash.dispose();
    });
  },

  testSetFlashVarUrlEncoding() {
    control.$replayAll();

    const flash = new FlashObject(FLASH_URL, domHelper);
    flash.setFlashVar('foo', 'bar and some extra spaces');
    flash.render();
    assertEquals(
        'foo=bar%20and%20some%20extra%20spaces',
        getFlashVarsFromElement(flash));
    flash.dispose();
  },

  testThrowsRequiredVersionOfFlashNotAvailable() {
    control.$replayAll();

    const flash = new FlashObject(FLASH_URL, domHelper);
    flash.setRequiredVersion('999.999.999');

    assertTrue(flash.hasRequiredVersion());

    assertThrows(() => {
      flash.render();
    });

    flash.dispose();
  },

  testIsLoadedForIE() {
    control.$replayAll();

    const flash = new FlashObject(FLASH_URL, domHelper);
    flash.render();
    assertNotThrows('isLoaded() should not throw exception', () => {
      flash.isLoaded();
    });
    flash.dispose();
  },

  testIsLoadedAfterDispose() {
    control.$replayAll();

    const flash = new FlashObject(FLASH_URL, domHelper);
    flash.render();
    // TODO(goto): find out a way to test the loadness of flash movies on
    // asynchronous tests. if debugger; is left here, the test pass. if removed
    // the test fails. that happens because flash needs some time to be
    // considered loaded, after flash.render() is called (like img.src i guess).
    // debugger;
    // assertTrue(flash.isLoaded());
    flash.dispose();
    assertFalse(flash.isLoaded());
  },

  testPropagatesEventsConsistently() {
    const event = control.createLooseMock(GoogEvent);

    // we expect any event to have its propagation stopped.
    event.stopPropagation();

    control.$replayAll();

    const flash = new FlashObject(FLASH_URL, domHelper);
    flash.render();
    event.target = flash.getElement();
    event.type = EventType.CLICK;
    testingEvents.fireBrowserEvent(event);
    flash.dispose();
  },

  testEventsGetsSinked() {
    let called = false;
    const flash = new FlashObject(FLASH_URL);
    const parent = dom.createElement(TagName.DIV);
    flash.render(parent);

    events.listen(parent, EventType.CLICK, (e) => {
      called = true;
    });

    assertFalse(called);

    testingEvents.fireClickSequence(flash.getElement());

    assertFalse(called);
    flash.dispose();
  },
});
