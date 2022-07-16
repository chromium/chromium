/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.FontSizeMonitorTest');
goog.setTestOnly();

const FontSizeMonitor = goog.require('goog.dom.FontSizeMonitor');
const GoogEvent = goog.require('goog.events.Event');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');
const userAgent = goog.require('goog.userAgent');

let monitor;

/** @suppress {visibility} suppression added to enable type checking */
function getResizeTarget() {
  return userAgent.IE ? monitor.sizeElement_ :
                        dom.getFrameContentWindow(monitor.sizeElement_);
}

testSuite({
  setUp() {
    monitor = new FontSizeMonitor();
  },

  tearDown() {
    monitor.dispose();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testFontSizeNoChange() {
    // This tests that firing the resize event without changing the font-size
    // does not trigger the event.

    let fired = false;
    events.listen(monitor, FontSizeMonitor.EventType.CHANGE, (e) => {
      fired = true;
    });

    const resizeEvent = new GoogEvent('resize', getResizeTarget());
    testingEvents.fireBrowserEvent(resizeEvent);

    assertFalse('The font size should not have changed', fired);
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testFontSizeChanged() {
    // One can trigger the iframe resize by changing the
    // document.body.style.fontSize but the event is fired asynchronously in
    // Firefox.  Instead, we just override the lastWidth_ to simulate that the
    // size changed.

    let fired = false;
    events.listen(monitor, FontSizeMonitor.EventType.CHANGE, (e) => {
      fired = true;
    });

    monitor.lastWidth_--;

    const resizeEvent = new GoogEvent('resize', getResizeTarget());
    testingEvents.fireBrowserEvent(resizeEvent);

    assertTrue('The font size should have changed', fired);
  },

  testCreateAndDispose() {
    const frameCount = window.frames.length;
    const iframeElementCount = dom.getElementsByTagName(TagName.IFRAME).length;
    const divElementCount = dom.getElementsByTagName(TagName.DIV).length;

    const monitor = new FontSizeMonitor();
    monitor.dispose();

    const newFrameCount = window.frames.length;
    const newIframeElementCount =
        dom.getElementsByTagName(TagName.IFRAME).length;
    const newDivElementCount = dom.getElementsByTagName(TagName.DIV).length;

    assertEquals(
        'There should be no trailing frames', frameCount, newFrameCount);
    assertEquals(
        'There should be no trailing iframe elements', iframeElementCount,
        newIframeElementCount);
    assertEquals(
        'There should be no trailing div elements', divElementCount,
        newDivElementCount);
  },

  testWithDomHelper() {
    const frameCount = window.frames.length;
    const iframeElementCount = dom.getElementsByTagName(TagName.IFRAME).length;
    const divElementCount = dom.getElementsByTagName(TagName.DIV).length;

    const monitor = new FontSizeMonitor(dom.getDomHelper());

    const newFrameCount = window.frames.length;
    const newIframeElementCount =
        dom.getElementsByTagName(TagName.IFRAME).length;
    const newDivElementCount = dom.getElementsByTagName(TagName.DIV).length;

    if (userAgent.IE) {
      assertEquals(
          'There should be one new div element', divElementCount + 1,
          newDivElementCount);
    } else {
      assertEquals(
          'There should be one new frame', frameCount + 1, newFrameCount);
      assertEquals(
          'There should be one new iframe element', iframeElementCount + 1,
          newIframeElementCount);
    }

    // Use the first iframe in the doc.  This is added in the HTML markup.
    const win = window.frames[0];
    const doc = win.document;
    doc.open();
    doc.write('<html><body></body></html>');
    doc.close();
    const domHelper = dom.getDomHelper(doc);

    const frameCount2 = win.frames.length;
    const iframeElementCount2 =
        dom.getElementsByTagName(TagName.IFRAME, doc).length;
    const divElementCount2 = dom.getElementsByTagName(TagName.DIV, doc).length;

    const monitor2 = new FontSizeMonitor(domHelper);

    const newFrameCount2 = win.frames.length;
    const newIframeElementCount2 =
        dom.getElementsByTagName(TagName.IFRAME, doc).length;
    const newDivElementCount2 =
        dom.getElementsByTagName(TagName.DIV, doc).length;

    if (userAgent.IE) {
      assertEquals(
          'There should be one new div element', divElementCount2 + 1,
          newDivElementCount2);
    } else {
      assertEquals(
          'There should be one new frame', frameCount2 + 1, newFrameCount2);
      assertEquals(
          'There should be one new iframe element', iframeElementCount2 + 1,
          newIframeElementCount2);
    }

    monitor.dispose();
    monitor2.dispose();
  },

  testEnsureThatDocIsOpenedForGecko() {
    const pr = new PropertyReplacer();
    pr.set(userAgent, 'GECKO', true);
    pr.set(userAgent, 'IE', false);

    let openCalled = false;
    let closeCalled = false;
    const instance = {
      document: {
        open: function() {
          openCalled = true;
        },
        close: function() {
          closeCalled = true;
        },
      },
      attachEvent: function() {},
    };

    pr.set(dom, 'getFrameContentWindow', () => instance);

    try {
      const monitor = new FontSizeMonitor();

      assertTrue('doc.open should have been called', openCalled);
      assertTrue('doc.close should have been called', closeCalled);

      monitor.dispose();
    } finally {
      pr.reset();
    }
  },

  testFirefox2WorkAroundFirefox3() {
    const pr = new PropertyReplacer();
    pr.set(userAgent, 'GECKO', true);
    pr.set(userAgent, 'IE', false);

    try {
      // 1.9 should clear iframes
      pr.set(userAgent, 'VERSION', '1.9');
      /**
       * @suppress {visibility,checkTypes,constantProperty} suppression added
       * to enable type checking
       */
      userAgent.isVersionOrHigherCache_ = {};

      const frameCount = window.frames.length;
      const iframeElementCount =
          dom.getElementsByTagName(TagName.IFRAME).length;
      const divElementCount = dom.getElementsByTagName(TagName.DIV).length;

      const monitor = new FontSizeMonitor();
      monitor.dispose();

      const newFrameCount = window.frames.length;
      const newIframeElementCount =
          dom.getElementsByTagName(TagName.IFRAME).length;
      const newDivElementCount = dom.getElementsByTagName(TagName.DIV).length;

      assertEquals(
          'There should be no trailing frames', frameCount, newFrameCount);
      assertEquals(
          'There should be no trailing iframe elements', iframeElementCount,
          newIframeElementCount);
      assertEquals(
          'There should be no trailing div elements', divElementCount,
          newDivElementCount);
    } finally {
      pr.reset();
    }
  },

});
