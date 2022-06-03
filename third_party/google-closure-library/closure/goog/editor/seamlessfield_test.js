/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.seamlessfield_test');
goog.setTestOnly();

const BrowserFeature = goog.require('goog.editor.BrowserFeature');
const DomHelper = goog.require('goog.dom.DomHelper');
const Field = goog.require('goog.editor.Field');
const MockClock = goog.require('goog.testing.MockClock');
const MockRange = goog.require('goog.testing.MockRange');
const Range = goog.require('goog.dom.Range');
const SafeHtml = goog.require('goog.html.SafeHtml');
const SeamlessField = goog.require('goog.editor.SeamlessField');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const functions = goog.require('goog.functions');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');

let fieldElem;
let fieldElemClone;

function createSeamlessIframe() {
  // NOTE(nicksantos): This is a reimplementation of
  // TR_EditableUtil.getIframeAttributes, but untangled for tests, and
  // specifically with what we need for blended mode.
  return dom.createDom(
      TagName.IFRAME, {'frameBorder': '0', 'style': 'padding:0;'});
}

/**
 * Initialize a new editable field for the field id 'field', with the given
 * innerHTML and styles.
 * @param {string} innerHTML html for the field contents.
 * @param {?Object} styles Key-value pairs for styles on the field.
 * @return {!SeamlessField} The field.
 */
function initSeamlessField(innerHTML, styles) {
  const field = new SeamlessField('field');
  fieldElem.innerHTML = innerHTML;
  style.setStyle(fieldElem, styles);
  return field;
}

/**
 * Make sure that the original field element for the given Field has
 * the same size before and after attaching the given iframe. If this is not
 * true, then the field will fidget while we're initializing the field,
 * and that's not what we want.
 * @param {?Field} fieldObj The field.
 * @param {?HTMLIFrameElement} iframe The iframe.
 * @suppress {visibility} suppression added to enable type checking
 */
function assertAttachSeamlessIframeSizesCorrectly(fieldObj, iframe) {
  const size = style.getSize(fieldObj.getOriginalElement());
  fieldObj.attachIframe(iframe);
  const newSize = style.getSize(fieldObj.getOriginalElement());

  assertEquals(size.width, newSize.width);
  assertEquals(size.height, newSize.height);
}

testSuite({
  setUp() {
    fieldElem = dom.getElement('field');
    fieldElemClone = fieldElem.cloneNode(true);
  },

  tearDown() {
    fieldElem.parentNode.replaceChild(fieldElemClone, fieldElem);
  },

  // the following tests check for blended iframe positioning. They really
  // only make sense on browsers without contentEditable.
  testBlankField() {
    if (!BrowserFeature.HAS_CONTENT_EDITABLE) {
      assertAttachSeamlessIframeSizesCorrectly(
          initSeamlessField('&nbsp;', {}), createSeamlessIframe());
    }
  },

  testFieldWithContent() {
    if (!BrowserFeature.HAS_CONTENT_EDITABLE) {
      assertAttachSeamlessIframeSizesCorrectly(
          initSeamlessField('Hi!', {}), createSeamlessIframe());
    }
  },

  testFieldWithPadding() {
    if (!BrowserFeature.HAS_CONTENT_EDITABLE) {
      assertAttachSeamlessIframeSizesCorrectly(
          initSeamlessField('Hi!', {'padding': '2px 5px'}),
          createSeamlessIframe());
    }
  },

  testFieldWithMargin() {
    if (!BrowserFeature.HAS_CONTENT_EDITABLE) {
      assertAttachSeamlessIframeSizesCorrectly(
          initSeamlessField('Hi!', {'margin': '2px 5px'}),
          createSeamlessIframe());
    }
  },

  testFieldWithBorder() {
    if (!BrowserFeature.HAS_CONTENT_EDITABLE) {
      assertAttachSeamlessIframeSizesCorrectly(
          initSeamlessField('Hi!', {'border': '2px 5px'}),
          createSeamlessIframe());
    }
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testFieldWithOverflow() {
    if (!BrowserFeature.HAS_CONTENT_EDITABLE) {
      assertAttachSeamlessIframeSizesCorrectly(
          initSeamlessField(
              ['1', '2', '3', '4', '5', '6', '7'].join('<p/>'),
              {'overflow': 'auto', 'position': 'relative', 'height': '20px'}),
          createSeamlessIframe());
      assertEquals(20, fieldElem.offsetHeight);
    }
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testFieldWithOverflowAndPadding() {
    if (!BrowserFeature.HAS_CONTENT_EDITABLE) {
      const blendedField =
          initSeamlessField(['1', '2', '3', '4', '5', '6', '7'].join('<p/>'), {
            'overflow': 'auto',
            'position': 'relative',
            'height': '20px',
            'padding': '2px 3px',
          });
      const blendedIframe = createSeamlessIframe();
      assertAttachSeamlessIframeSizesCorrectly(blendedField, blendedIframe);
      assertEquals(24, fieldElem.offsetHeight);
    }
  },

  testIframeHeightGrowsOnWrap() {
    if (!BrowserFeature.HAS_CONTENT_EDITABLE) {
      const clock = new MockClock(true);
      let blendedField;
      try {
        blendedField = initSeamlessField(
            '', {'border': '1px solid black', 'height': '20px'});
        blendedField.makeEditable();
        blendedField.setSafeHtml(
            false,
            SafeHtml.htmlEscape('Content that should wrap after resize.'));

        // Ensure that the field was fully loaded and sized before measuring.
        clock.tick(1);

        // Capture starting heights.
        /** @suppress {visibility} suppression added to enable type checking */
        const unwrappedIframeHeight =
            blendedField.getEditableIframe().offsetHeight;

        // Resize the field such that the text should wrap.
        fieldElem.style.width = '200px';
        blendedField.doFieldSizingGecko();

        // Iframe should grow as a result.
        /** @suppress {visibility} suppression added to enable type checking */
        const wrappedIframeHeight =
            blendedField.getEditableIframe().offsetHeight;
        assertTrue(
            'Wrapped text should cause iframe to grow - initial height: ' +
                unwrappedIframeHeight +
                ', wrapped height: ' + wrappedIframeHeight,
            wrappedIframeHeight > unwrappedIframeHeight);
      } finally {
        blendedField.dispose();
        clock.dispose();
      }
    }
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testDispatchIframeResizedForWrapperHeight() {
    if (!BrowserFeature.HAS_CONTENT_EDITABLE) {
      const clock = new MockClock(true);
      const blendedField = initSeamlessField('Hi!', {'border': '2px 5px'});
      const iframe = createSeamlessIframe();
      blendedField.attachIframe(iframe);

      let resizeCalled = false;
      events.listenOnce(blendedField, Field.EventType.IFRAME_RESIZED, () => {
        resizeCalled = true;
      });

      try {
        blendedField.makeEditable();
        blendedField.setSafeHtml(
            false,
            SafeHtml.htmlEscape('Content that should wrap after resize.'));

        // Ensure that the field was fully loaded and sized before measuring.
        clock.tick(1);

        assertFalse('Iframe resize must not be dispatched yet', resizeCalled);

        // Resize the field such that the text should wrap.
        fieldElem.style.width = '200px';
        blendedField.sizeIframeToWrapperGecko_();
        assertTrue(
            'Iframe resize must be dispatched for Wrapper', resizeCalled);
      } finally {
        blendedField.dispose();
        clock.dispose();
      }
    }
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testDispatchIframeResizedForBodyHeight() {
    if (!BrowserFeature.HAS_CONTENT_EDITABLE) {
      const clock = new MockClock(true);
      const blendedField = initSeamlessField('Hi!', {'border': '2px 5px'});
      const iframe = createSeamlessIframe();
      blendedField.attachIframe(iframe);

      let resizeCalled = false;
      events.listenOnce(blendedField, Field.EventType.IFRAME_RESIZED, () => {
        resizeCalled = true;
      });

      try {
        blendedField.makeEditable();
        blendedField.setSafeHtml(
            false,
            SafeHtml.htmlEscape('Content that should wrap after resize.'));

        // Ensure that the field was fully loaded and sized before measuring.
        clock.tick(1);

        assertFalse('Iframe resize must not be dispatched yet', resizeCalled);

        // Resize the field to a different body height.
        /** @suppress {visibility} suppression added to enable type checking */
        const bodyHeight = blendedField.getIframeBodyHeightGecko_();
        /** @suppress {visibility} suppression added to enable type checking */
        blendedField.getIframeBodyHeightGecko_ = () => bodyHeight + 1;
        blendedField.sizeIframeToBodyHeightGecko_();
        assertTrue('Iframe resize must be dispatched for Body', resizeCalled);
      } finally {
        blendedField.dispose();
        clock.dispose();
      }
    }
  },

  /**
     @suppress {visibility,missingProperties} suppression added to enable type
     checking
   */
  testDispatchBlur() {
    if (!BrowserFeature.HAS_CONTENT_EDITABLE &&
        !BrowserFeature.CLEARS_SELECTION_WHEN_FOCUS_LEAVES) {
      const blendedField = initSeamlessField('Hi!', {'border': '2px 5px'});
      const iframe = createSeamlessIframe();
      blendedField.attachIframe(iframe);

      let blurCalled = false;
      events.listenOnce(blendedField, Field.EventType.BLUR, () => {
        blurCalled = true;
      });

      const clearSelection = Range.clearSelection;
      let cleared = false;
      let clearedWindow;
      /** @suppress {visibility} suppression added to enable type checking */
      blendedField.editableDomHelper = new DomHelper();
      /** @suppress {visibility} suppression added to enable type checking */
      blendedField.editableDomHelper.getWindow =
          functions.constant(iframe.contentWindow);
      const mockRange = new MockRange();
      blendedField.getRange = () => mockRange;
      Range.clearSelection = (opt_window) => {
        clearSelection(opt_window);
        cleared = true;
        clearedWindow = opt_window;
      };
      const clock = new MockClock(true);

      mockRange.collapse(true);
      mockRange.select();
      mockRange.$replay();
      blendedField.dispatchBlur();
      clock.tick(1);

      assertTrue('Blur must be dispatched.', blurCalled);
      assertTrue('Selection must be cleared.', cleared);
      assertEquals(
          'Selection must be cleared in iframe', iframe.contentWindow,
          clearedWindow);
      mockRange.$verify();
      clock.dispose();
    }
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSetMinHeight() {
    if (!BrowserFeature.HAS_CONTENT_EDITABLE) {
      const clock = new MockClock(true);
      let field;
      try {
        field = initSeamlessField(
            ['1', '2', '3', '4', '5', '6', '7'].join('<p/>'),
            {'position': 'relative', 'height': '60px'});

        // Initially create and size iframe.
        const iframe = createSeamlessIframe();
        field.attachIframe(iframe);
        field.iframeFieldLoadHandler(iframe, '', {});
        // Need to process timeouts set by load handlers.
        clock.tick(1000);

        const normalHeight = style.getSize(iframe).height;

        let delayedChangeCalled = false;
        events.listen(field, Field.EventType.DELAYEDCHANGE, () => {
          delayedChangeCalled = true;
        });

        // Test that min height is obeyed.
        field.setMinHeight(30);
        clock.tick(1000);
        assertEquals(
            'Iframe height must match min height.', 30,
            style.getSize(iframe).height);
        assertFalse(
            'Setting min height must not cause delayed change event.',
            delayedChangeCalled);

        // Test that min height doesn't shrink field.
        field.setMinHeight(0);
        clock.tick(1000);
        assertEquals(normalHeight, style.getSize(iframe).height);
        assertFalse(
            'Setting min height must not cause delayed change event.',
            delayedChangeCalled);
      } finally {
        field.dispose();
        clock.dispose();
      }
    }
  },

  /** @bug 1649967 This code used to throw a JavaScript error. */
  testSetMinHeightWithNoIframe() {
    if (BrowserFeature.HAS_CONTENT_EDITABLE) {
      let field;
      try {
        field = initSeamlessField('&nbsp;', {});
        field.makeEditable();
        field.setMinHeight(30);
      } finally {
        field.dispose();
      }
    }
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testStartChangeEvents() {
    if (BrowserFeature.USE_MUTATION_EVENTS) {
      const clock = new MockClock(true);

      let field;
      try {
        field = initSeamlessField('&nbsp;', {});
        field.makeEditable();

        let changeCalled = false;
        events.listenOnce(field, Field.EventType.CHANGE, () => {
          changeCalled = true;
        });

        let delayedChangeCalled = false;
        events.listenOnce(field, Field.EventType.CHANGE, () => {
          delayedChangeCalled = true;
        });

        field.stopChangeEvents(true, true);
        if (field.changeTimerGecko_) {
          field.changeTimerGecko_.start();
        }

        field.startChangeEvents();
        clock.tick(1000);

        assertFalse(changeCalled);
        assertFalse(delayedChangeCalled);
      } finally {
        clock.dispose();
        field.dispose();
      }
    }
  },

  testManipulateDom() {
    // Test in blended field since that is what fires change events.
    const editableField = initSeamlessField('&nbsp;', {});
    const clock = new MockClock(true);

    let delayedChangeCalled = 0;
    events.listen(editableField, Field.EventType.DELAYEDCHANGE, () => {
      delayedChangeCalled++;
    });

    assertFalse(editableField.isLoaded());
    editableField.manipulateDom(goog.nullFunction);
    clock.tick(1000);
    assertEquals(
        'Must not fire delayed change events if field is not loaded.', 0,
        delayedChangeCalled);

    editableField.makeEditable();
    const usesIframe = editableField.usesIframe();

    try {
      editableField.manipulateDom(goog.nullFunction);
      clock.tick(1000);  // Wait for delayed change to fire.
      assertEquals(
          'By default must fire a single delayed change event.', 1,
          delayedChangeCalled);

      editableField.manipulateDom(goog.nullFunction, true);
      clock.tick(1000);  // Wait for delayed change to fire.
      assertEquals(
          'Must prevent all delayed change events.', 1, delayedChangeCalled);

      editableField.manipulateDom(function() {
        this.handleChange();
        this.handleChange();
        if (this.changeTimerGecko_) {
          this.changeTimerGecko_.fire();
        }

        this.dispatchDelayedChange_();
        this.delayedChangeTimer_.fire();
      }, false, editableField);
      clock.tick(1000);  // Wait for delayed change to fire.
      assertEquals(
          'Must ignore dispatch delayed change called within func.', 2,
          delayedChangeCalled);
    } finally {
      // Ensure we always uninstall the mock clock and dispose of everything.
      editableField.dispose();
      clock.dispose();
    }
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAttachIframe() {
    const blendedField = initSeamlessField('Hi!', {});
    const iframe = createSeamlessIframe();
    try {
      blendedField.attachIframe(iframe);
    } catch (err) {
      fail('Error occurred while attaching iframe.');
    }
  },
});
